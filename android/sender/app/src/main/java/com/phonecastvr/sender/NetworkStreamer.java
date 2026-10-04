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
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

final class NetworkStreamer {
    interface Listener {
        void onConnectionChanged(boolean connected, String message);
        void onKeyFrameNeeded();
        void onRemoteInput(RemoteInputEvent event);
    }

    private static final String TAG = "PhoneCastNetwork";
    private static final int CONNECT_TIMEOUT_MILLIS = 3000;
    private static final int MAX_BACKOFF_MILLIS = 5000;

    private final String host;
    private final int port;
    private final String pairCode;
    private final Listener listener;
    private final ArrayBlockingQueue<Packet> frames = new ArrayBlockingQueue<>(3);
    private final AtomicBoolean running = new AtomicBoolean();
    private final AtomicBoolean waitingForKeyFrame = new AtomicBoolean(true);
    private final AtomicLong droppedFrames = new AtomicLong();
    private final AtomicLong roundTripMicros = new AtomicLong(-1);
    private volatile Packet latestConfig;
    private volatile Packet latestKeyFrame;
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

    void offerConfig(byte[] payload, int width, int height) {
        droppedFrames.addAndGet(frames.size());
        frames.clear();
        waitingForKeyFrame.set(true);
        latestKeyFrame = null;
        latestConfig = new Packet(StreamProtocol.TYPE_VIDEO_CONFIG, 0, 0, 0,
                width, height, payload);
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
        if (frames.offer(packet)) return;

        int discarded = frames.size();
        frames.clear();
        droppedFrames.addAndGet(discarded);
        if (keyFrame) {
            frames.offer(packet);
            return;
        }
        droppedFrames.incrementAndGet();
        if (waitingForKeyFrame.compareAndSet(false, true)) listener.onKeyFrameNeeded();
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
                Thread reader = new Thread(() -> readReceiverMessages(input),
                        "phonecast-network-replies");
                reader.setDaemon(true);
                reader.start();
                StreamProtocol.write(output, StreamProtocol.TYPE_HELLO, 0, 0,
                        System.nanoTime() / 1000L, 0, 0,
                        pairCode.getBytes(StandardCharsets.US_ASCII));
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
                waitingForKeyFrame.set(true);
                listener.onKeyFrameNeeded();
                backoffMillis = 250;
                long nextPingNanos = 0;

                while (running.get()) {
                    Packet currentConfig = latestConfig;
                    if (currentConfig != null && currentConfig != sentConfig) {
                        currentConfig.write(output);
                        sentConfig = currentConfig;
                        output.flush();
                    }
                    long now = System.nanoTime();
                    if (now >= nextPingNanos) {
                        StreamProtocol.write(output, StreamProtocol.TYPE_PING, 0, 0,
                                now / 1000L, 0, 0, new byte[0]);
                        output.flush();
                        nextPingNanos = now + 1_000_000_000L;
                    }
                    Packet packet = frames.poll(100, TimeUnit.MILLISECONDS);
                    if (packet == null) continue;
                    packet.write(output);
                    output.flush();
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
            }
        }
        listener.onConnectionChanged(false, "Streaming stopped");
    }

    private void readReceiverMessages(DataInputStream input) {
        try {
            while (running.get()) {
                StreamProtocol.Header header = StreamProtocol.readHeader(input);
                byte[] payload = new byte[header.payloadSize];
                if (header.payloadSize > 0) input.readFully(payload);
                if (header.type == StreamProtocol.TYPE_PONG) {
                    roundTripMicros.set(Math.max(0L,
                            System.nanoTime() / 1000L - header.timestampMicros));
                } else if (header.type == StreamProtocol.TYPE_REQUEST_KEY_FRAME) {
                    droppedFrames.addAndGet(frames.size());
                    frames.clear();
                    waitingForKeyFrame.set(true);
                    listener.onKeyFrameNeeded();
                } else if (header.type == StreamProtocol.TYPE_REMOTE_INPUT) {
                    listener.onRemoteInput(StreamProtocol.parseRemoteInput(payload, header.sequence));
                }
            }
        } catch (IOException ignored) {
            // The writer loop owns reconnect behavior and reports connection state.
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
