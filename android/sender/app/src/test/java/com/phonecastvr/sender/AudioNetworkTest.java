package com.phonecastvr.sender;

import org.junit.Test;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import static org.junit.Assert.*;

public class AudioNetworkTest {
    private static final class Listener implements NetworkStreamer.Listener {
        final AtomicBoolean capable = new AtomicBoolean();
        @Override public void onConnectionChanged(boolean connected,String message) {}
        @Override public void onKeyFrameNeeded() {}
        @Override public void onRemoteInput(RemoteInputEvent event) {}
        @Override public void onNotificationOpen(long token) {}
        @Override public void onAudioCapability(boolean supported) { capable.set(supported); }
    }
    private static final class Received {
        final StreamProtocol.Header header;
        final byte[] payload;
        Received(DataInputStream input) throws Exception {
            header = StreamProtocol.readHeader(input);
            payload = new byte[header.payloadSize]; input.readFully(payload);
        }
    }
    @Test public void oldReceiverNeverReceivesAudioAndVideoContinues() throws Exception {
        exercise(false);
    }
    @Test public void capableReceiverGetsConfigPcmAndImmediateOptOutWithoutClosingVideo() throws Exception {
        exercise(true);
    }
    private void exercise(boolean supported) throws Exception {
        try (ServerSocket receiver = new ServerSocket(0)) {
            Listener listener = new Listener();
            NetworkStreamer sender = new NetworkStreamer("127.0.0.1",receiver.getLocalPort(),"123456",listener);
            AtomicReference<Throwable> failure = new AtomicReference<>();
            CountDownLatch negotiation = new CountDownLatch(1), media = new CountDownLatch(1),
                    stopped = new CountDownLatch(1), videoReceived = new CountDownLatch(1), cleanup = new CountDownLatch(1);
            Thread peer = new Thread(() -> {
                try (Socket socket = receiver.accept()) {
                    socket.setSoTimeout(3000);
                    DataInputStream input = new DataInputStream(socket.getInputStream());
                    DataOutputStream output = new DataOutputStream(socket.getOutputStream());
                    assertEquals(StreamProtocol.TYPE_HELLO,new Received(input).header.type);
                    Received ping = new Received(input);
                    assertEquals(StreamProtocol.TYPE_PING,ping.header.type);
                    assertTrue(AudioProtocol.hasCapability(ping.payload));
                    StreamProtocol.write(output,StreamProtocol.TYPE_PONG,0,0,ping.header.timestampMicros,
                            0,0,supported ? AudioProtocol.capability() : new byte[0]); output.flush();
                    negotiation.countDown();
                    if (supported) {
                        boolean config = false, frame = false;
                        while (!frame) {
                            Received message = new Received(input);
                            if (message.header.type == AudioProtocol.CONFIG) {
                                assertArrayEquals(AudioProtocol.config(9),message.payload); config = true;
                            } else if (message.header.type == AudioProtocol.FRAME) {
                                assertTrue(config); assertEquals(1932,message.payload.length); frame = true;
                            }
                        }
                        media.countDown();
                        boolean off = false;
                        while (!off) {
                            Received message = new Received(input);
                            if (message.header.type == AudioProtocol.STATUS && message.payload[1] == AudioProtocol.OFF) off = true;
                        }
                        stopped.countDown();
                    }
                    boolean video = false;
                    while (!video) {
                        Received message = new Received(input);
                        if (!supported) assertTrue("Old peer must see only original types",message.header.type <= 11);
                        if (message.header.type == StreamProtocol.TYPE_VIDEO_FRAME) video = true;
                    }
                    if (!supported) media.countDown();
                    videoReceived.countDown();
                    cleanup.await(3,TimeUnit.SECONDS);
                } catch (Throwable error) { failure.set(error); negotiation.countDown(); media.countDown(); stopped.countDown(); }
            });
            peer.start();
            try {
                sender.setAudioRequested(true); sender.start();
                assertTrue(negotiation.await(3,TimeUnit.SECONDS));
                if (supported) {
                    long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(2);
                    while (!listener.capable.get() && System.nanoTime() < deadline) Thread.sleep(2);
                    assertTrue(listener.capable.get());
                } else { Thread.sleep(30); assertFalse(sender.audioCapable()); }
                sender.beginAudio(9); sender.audioStatus(9,AudioProtocol.ACTIVE);
                sender.offerAudio(9,0,1_000_000L,AudioProtocol.block(9,new short[960]));
                if (supported) {
                    assertTrue(media.await(3,TimeUnit.SECONDS)); sender.endAudio(9,AudioProtocol.OFF);
                    assertTrue(stopped.await(3,TimeUnit.SECONDS));
                    sender.offerAudio(9,1,1_010_000L,AudioProtocol.block(9,new short[960])); // Old epoch discarded.
                }
                sender.offerConfig(new byte[]{1},2,2);
                sender.offerFrame(new byte[]{2},StreamProtocol.FLAG_KEY_FRAME,0,1000000,2,2);
                assertTrue(videoReceived.await(3,TimeUnit.SECONDS));
                sender.stop(); cleanup.countDown(); peer.join(3500);
                assertFalse(peer.isAlive());
                if (failure.get() != null) throw new AssertionError(failure.get());
            } finally { sender.stop(); cleanup.countDown(); peer.join(3500); }
        }
    }
}
