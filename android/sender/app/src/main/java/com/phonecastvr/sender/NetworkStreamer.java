package com.phonecastvr.sender;

import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

final class NetworkStreamer {
    interface Listener {
        void onConnectionChanged(boolean connected, String message);
        void onKeyFrameNeeded();
        void onRemoteInput(RemoteInputEvent event);
        void onNotificationOpen(long actionToken);
        void onAudioCapability(boolean supported);
    }

    private static final String TAG = "PhoneCastNetwork";
    private static final int CONNECT_TIMEOUT_MILLIS = 3000;
    private static final int MAX_BACKOFF_MILLIS = 5000;

    private final String host;
    private final int port;
    private final String pairCode;
    private final Listener listener;
    private final ArrayBlockingQueue<Packet> frames = new ArrayBlockingQueue<>(3);
    private final ArrayBlockingQueue<Packet> notifications = new ArrayBlockingQueue<>(8);
    private final ArrayBlockingQueue<Packet> audioFrames = new ArrayBlockingQueue<>(10);
    private final Semaphore wakeup = new Semaphore(0);
    private final Object audioLock = new Object();
    private volatile boolean audioRequested, audioSupported;
    private volatile Packet latestAudioConfig, latestAudioStatus;
    private long audioEpoch;
    private final AtomicLong audioDropped = new AtomicLong();
    private final AtomicBoolean running = new AtomicBoolean();
    private final AtomicBoolean waitingForKeyFrame = new AtomicBoolean(true);
    private final AtomicLong droppedFrames = new AtomicLong();
    private final AtomicLong roundTripMicros = new AtomicLong(-1);
    private final AtomicLong notificationSequence = new AtomicLong();
    private volatile Packet latestConfig;
    private volatile Packet latestKeyFrame;
    private volatile Packet latestRemoteControlStatus;
    private volatile Socket socket;
    private Thread thread;

    NetworkStreamer(String host, int port, String pairCode, Listener listener) {
        this.host = host;
        this.port = port;
        this.pairCode = pairCode;
        this.listener = listener;
    }

    void start() {
        if (!running.compareAndSet(false, true)) return;
        thread = new Thread(this::run, "phonecast-network");
        thread.start();
    }

    private void wake() { if (wakeup.availablePermits() == 0) wakeup.release(); }
    void setAudioRequested(boolean enabled) { audioRequested = enabled; wake(); }
    boolean audioCapable() { return audioSupported && audioRequested && running.get(); }
    long audioDropped() { return audioDropped.get(); }
    void beginAudio(long epoch) {
        synchronized (audioLock) {
            if (!audioCapable()) return;
            audioEpoch = epoch; audioFrames.clear(); latestAudioStatus = null;
            latestAudioConfig = new Packet(AudioProtocol.CONFIG,0,0,0,0,0,AudioProtocol.config(epoch));
        }
        wake();
    }
    void offerAudio(long epoch, long sequence, long pts, byte[] payload) {
        synchronized (audioLock) {
            if (epoch != audioEpoch || !audioCapable()) return;
            Packet packet = new Packet(AudioProtocol.FRAME,0,sequence,pts,0,0,payload);
            if (!audioFrames.offer(packet)) { audioFrames.poll(); audioDropped.incrementAndGet(); audioFrames.offer(packet); }
        }
        wake();
    }
    void audioStatus(long epoch, int status) {
        synchronized (audioLock) {
            if (epoch != audioEpoch || !audioCapable()) return;
            latestAudioStatus = new Packet(AudioProtocol.STATUS,0,0,0,0,0,AudioProtocol.status(epoch,status));
        }
        wake();
    }
    void endAudio(long epoch, int status) {
        synchronized (audioLock) {
            if (epoch != audioEpoch) return;
            audioFrames.clear(); audioEpoch = 0;
            latestAudioStatus = new Packet(AudioProtocol.STATUS,0,0,0,0,0,AudioProtocol.status(epoch,status));
        }
        wake();
    }

    void offerConfig(byte[] payload, int width, int height) {
        droppedFrames.addAndGet(frames.size());
        frames.clear();
        waitingForKeyFrame.set(true);
        latestKeyFrame = null;
        latestConfig = new Packet(StreamProtocol.TYPE_VIDEO_CONFIG, 0, 0, 0,
                width, height, payload);
        wake();
    }

    void offerFrame(byte[] payload, int flags, long sequence, long timestampMicros,
                    int width, int height) {
        boolean keyFrame = (flags & StreamProtocol.FLAG_KEY_FRAME) != 0;
        if (waitingForKeyFrame.get() && !keyFrame) {
            droppedFrames.incrementAndGet();
            return;
        }
        if (keyFrame) waitingForKeyFrame.set(false);

        Packet packet = new Packet(StreamProtocol.TYPE_VIDEO_FRAME, flags, sequence,
                timestampMicros, width, height, payload);
        if (keyFrame) latestKeyFrame = packet;
        if (frames.offer(packet)) { wake(); return; }

        int discarded = frames.size();
        frames.clear();
        droppedFrames.addAndGet(discarded);
        if (keyFrame) {
            frames.offer(packet);
            wake();
            return;
        }
        droppedFrames.incrementAndGet();
        if (waitingForKeyFrame.compareAndSet(false, true)) listener.onKeyFrameNeeded();
    }

    void offerNotification(byte[] payload, long postedAtMillis) {
        if (!running.get() || payload == null || payload.length == 0) return;
        Packet packet = new Packet(StreamProtocol.TYPE_NOTIFICATION, 0,
                notificationSequence.getAndIncrement(), postedAtMillis * 1000L,
                0, 0, payload);
        if (notifications.offer(packet)) { wake(); return; }
        notifications.poll();
        notifications.offer(packet);
        wake();
    }

    void clearNotifications() {
        notifications.clear();
    }

    void offerRemoteControlStatus(boolean appEnabled, boolean accessibilityEnabled) {
        latestRemoteControlStatus = new Packet(StreamProtocol.TYPE_REMOTE_CONTROL_STATUS,
                0, 0, 0, 0, 0,
                StreamProtocol.remoteControlStatusPayload(appEnabled, accessibilityEnabled));
        wake();
    }

    long droppedFrames() {
        return droppedFrames.get();
    }

    long roundTripMicros() {
        return roundTripMicros.get();
    }

    void stop() {
        if (!running.getAndSet(false)) return;
        closeSocket();
        if (thread != null) thread.interrupt();
    }

    private void run() {
        int backoffMillis = 250;
        while (running.get()) {
            try (Socket activeSocket = new Socket()) {
                socket = activeSocket;
                activeSocket.setTcpNoDelay(true);
                activeSocket.connect(new InetSocketAddress(host, port), CONNECT_TIMEOUT_MILLIS);
                DataOutputStream output = new DataOutputStream(
                        new BufferedOutputStream(activeSocket.getOutputStream(), 256 * 1024));
                DataInputStream input = new DataInputStream(
                        new BufferedInputStream(activeSocket.getInputStream(), 4096));
                synchronized (audioLock) {
                    audioSupported = false; audioEpoch = 0; audioFrames.clear();
                    latestAudioConfig = null; latestAudioStatus = null;
                }
                listener.onAudioCapability(false);
                Thread reader = new Thread(() -> readReceiverMessages(input,activeSocket),
                        "phonecast-network-replies");
                reader.setDaemon(true);
                reader.start();
                StreamProtocol.write(output, StreamProtocol.TYPE_HELLO, 0, 0,
                        System.nanoTime() / 1000L, 0, 0,
                        pairCode.getBytes(StandardCharsets.US_ASCII));
                Packet sentRemoteControlStatus = latestRemoteControlStatus;
                if (sentRemoteControlStatus != null) sentRemoteControlStatus.write(output);
                Packet sentConfig = latestConfig;
                if (sentConfig != null) sentConfig.write(output);
                Packet cachedKeyFrame = latestKeyFrame;
                if (cachedKeyFrame != null && sentConfig != null &&
                        cachedKeyFrame.width == sentConfig.width &&
                        cachedKeyFrame.height == sentConfig.height) {
                    cachedKeyFrame.write(output);
                }
                output.flush();
                listener.onConnectionChanged(true, "Connected to " + host + ':' + port);
                droppedFrames.addAndGet(frames.size());
                frames.clear();
                notifications.clear();
                waitingForKeyFrame.set(true);
                listener.onKeyFrameNeeded();
                backoffMillis = 250;
                long nextPingNanos = 0;
                Packet sentAudioConfig = null, sentAudioStatus = null;

                while (running.get()) {
                    Packet currentRemoteControlStatus = latestRemoteControlStatus;
                    if (currentRemoteControlStatus != null &&
                            currentRemoteControlStatus != sentRemoteControlStatus) {
                        currentRemoteControlStatus.write(output);
                        sentRemoteControlStatus = currentRemoteControlStatus;
                        output.flush();
                    }
                    Packet currentConfig = latestConfig;
                    if (currentConfig != null && currentConfig != sentConfig) {
                        currentConfig.write(output);
                        sentConfig = currentConfig;
                        output.flush();
                    }
                    long now = System.nanoTime();
                    if (now >= nextPingNanos) {
                        StreamProtocol.write(output, StreamProtocol.TYPE_PING, 0, 0,
                                now / 1000L, 0, 0, audioRequested ? AudioProtocol.capability() : new byte[0]);
                        output.flush();
                        nextPingNanos = now + 1_000_000_000L;
                    }
                    boolean wrote = false;
                    Packet audioConfig, audioStatus, audioFrame;
                    synchronized (audioLock) {
                        audioConfig = latestAudioConfig; audioStatus = latestAudioStatus;
                        audioFrame = audioFrames.poll();
                    }
                    if (audioSupported && audioConfig != null && audioConfig != sentAudioConfig) {
                        audioConfig.write(output); sentAudioConfig = audioConfig; wrote = true;
                    }
                    if (audioSupported && audioStatus != null && audioStatus != sentAudioStatus) {
                        audioStatus.write(output); sentAudioStatus = audioStatus; wrote = true;
                    }
                    if (audioSupported && audioFrame != null) {
                        // Never replay stale sound after TCP send blocking.
                        if (System.nanoTime() - audioFrame.createdNanos < 100_000_000L) {
                            audioFrame.write(output); wrote = true;
                        } else audioDropped.incrementAndGet();
                    }
                    Packet notification = notifications.poll();
                    if (notification != null) { notification.write(output); wrote = true; }
                    Packet packet = frames.poll();
                    if (packet != null) { packet.write(output); wrote = true; }
                    if (wrote) output.flush();
                    else { wakeup.tryAcquire(100,TimeUnit.MILLISECONDS); wakeup.drainPermits(); }
                }
                StreamProtocol.write(output, StreamProtocol.TYPE_END_STREAM, 0, 0,
                        0, 0, 0, new byte[0]);
                output.flush();
            } catch (IOException error) {
                if (running.get()) {
                    listener.onConnectionChanged(false, "Waiting for receiver at " + host + ':' + port);
                    Log.w(TAG, "Receiver connection failed: " + error.getMessage());
                    sleep(backoffMillis);
                    backoffMillis = Math.min(MAX_BACKOFF_MILLIS, backoffMillis * 2);
                }
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
            } finally {
                socket = null;
                synchronized (audioLock) {
                    audioSupported = false; audioEpoch = 0; audioFrames.clear();
                    latestAudioConfig = null; latestAudioStatus = null;
                }
                listener.onAudioCapability(false);
            }
        }
        listener.onConnectionChanged(false, "Streaming stopped");
    }

    private void readReceiverMessages(DataInputStream input, Socket owner) {
        try {
            while (running.get() && socket == owner) {
                StreamProtocol.Header header = StreamProtocol.readHeader(input);
                byte[] payload = new byte[header.payloadSize];
                if (header.payloadSize > 0) input.readFully(payload);
                if (socket != owner) return;
                if (header.type == StreamProtocol.TYPE_PONG) {
                    if (audioRequested && !audioSupported && AudioProtocol.hasCapability(payload)) {
                        audioSupported = true;
                        listener.onAudioCapability(true);
                    }
                    roundTripMicros.set(Math.max(0L,
                            System.nanoTime() / 1000L - header.timestampMicros));
                } else if (header.type == StreamProtocol.TYPE_REQUEST_KEY_FRAME) {
                    droppedFrames.addAndGet(frames.size());
                    frames.clear();
                    waitingForKeyFrame.set(true);
                    listener.onKeyFrameNeeded();
                } else if (header.type == StreamProtocol.TYPE_REMOTE_INPUT) {
                    listener.onRemoteInput(StreamProtocol.parseRemoteInput(payload, header.sequence));
                } else if (header.type == StreamProtocol.TYPE_NOTIFICATION_OPEN &&
                        header.payloadSize == 0) {
                    listener.onNotificationOpen(header.sequence);
                }
            }
        } catch (IOException ignored) {
            // Wake a blocked writer and stop capture promptly on reader disconnect.
            if (socket == owner) {
                audioSupported = false;
                listener.onAudioCapability(false);
                try { owner.close(); } catch (IOException closing) { /* Already closed. */ }
                wake();
            }
        }
    }

    private void closeSocket() {
        Socket activeSocket = socket;
        if (activeSocket == null) return;
        try {
            activeSocket.close();
        } catch (IOException ignored) {
            // Closing a connection is best effort.
        }
    }

    private static void sleep(int millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException ignored) {
            Thread.currentThread().interrupt();
        }
    }

    private static final class Packet {
        final int type;
        final int flags;
        final long sequence;
        final long timestampMicros;
        final int width;
        final int height;
        final byte[] payload;
        final long createdNanos = System.nanoTime();

        Packet(int type, int flags, long sequence, long timestampMicros,
               int width, int height, byte[] payload) {
            this.type = type;
            this.flags = flags;
            this.sequence = sequence;
            this.timestampMicros = timestampMicros;
            this.width = width;
            this.height = height;
            this.payload = payload;
        }

        void write(DataOutputStream output) throws IOException {
            StreamProtocol.write(output, type, flags, sequence, timestampMicros,
                    width, height, payload);
        }
    }
}
