package com.phonecastvr.sender;
import org.junit.Test;
import static org.junit.Assert.*;
import java.net.ServerSocket;
import java.net.Socket;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.function.BooleanSupplier;

public class NetworkFeedbackTest {
    private static void await(BooleanSupplier condition) throws Exception {
        long end = System.nanoTime() + 3000000000L;
        while (!condition.getAsBoolean() && System.nanoTime() < end) Thread.sleep(10);
        assertTrue(condition.getAsBoolean());
    }
    private static NetworkStreamer sender(int port) { return sender(port, message -> {}); }
    private static NetworkStreamer sender(int port, java.util.function.Consumer<String> observe) {
        return new NetworkStreamer("127.0.0.1", port, "123456", new NetworkStreamer.Listener() {
            public void onConnectionChanged(boolean ready, String message) { observe.accept(message); }
            public void onKeyFrameNeeded() {}
            public void onRemoteInput(RemoteInputEvent event) {}
            public void onNotificationOpen(long token) {}
            public void onAudioCapability(boolean supported) {}
        });
    }
    private static long ping(Socket socket) throws Exception {
        DataInputStream input = new DataInputStream(socket.getInputStream());
        while (true) {
            StreamProtocol.Header header = StreamProtocol.readHeader(input);
            input.readFully(new byte[header.payloadSize]);
            if (header.type == StreamProtocol.TYPE_PING) return header.timestampMicros;
        }
    }
    @Test public void tcpIsNotConfirmationAndDisconnectReconnectResetReadiness() throws Exception {
        try (ServerSocket receiver = new ServerSocket(0)) {
            receiver.setSoTimeout(4000);
            NetworkStreamer sender = sender(receiver.getLocalPort());
            sender.start();
            try {
                try (Socket first = receiver.accept()) {
                    first.setSoTimeout(3000);
                    long timestamp = ping(first);
                    await(() -> sender.connectionMessage().equals(ConnectionFeedback.WAITING));
                    assertFalse(sender.receiverResponsive());
                    DataOutputStream output = new DataOutputStream(first.getOutputStream());
                    StreamProtocol.write(output, StreamProtocol.TYPE_PONG,0,0,0,0,0,null); output.flush();
                    Thread.sleep(50); assertFalse(sender.receiverResponsive());
                    StreamProtocol.write(output, StreamProtocol.TYPE_PONG,0,0,timestamp,0,0,null); output.flush();
                    await(sender::receiverResponsive);
                    assertTrue(sender.roundTripMicros() >= 0);
                }
                await(() -> !sender.receiverResponsive());
                try (Socket second = receiver.accept()) {
                    second.setSoTimeout(3000);
                    long timestamp = ping(second);
                    assertFalse(sender.receiverResponsive());
                    assertEquals(-1, sender.roundTripMicros());
                    DataOutputStream output = new DataOutputStream(second.getOutputStream());
                    StreamProtocol.write(output, StreamProtocol.TYPE_PONG,0,0,timestamp,0,0,null); output.flush();
                    await(sender::receiverResponsive);
                }
            } finally { sender.stop(); }
        }
    }
    @Test public void unavailableReceiverIsWarningNotGreen() throws Exception {
        int port;
        try (ServerSocket unused = new ServerSocket(0)) { port = unused.getLocalPort(); }
        NetworkStreamer sender = sender(port); sender.start();
        try {
            await(() -> sender.connectionMessage().contains("Cannot reach"));
            assertFalse(sender.receiverResponsive());
            assertEquals(-1, sender.roundTripMicros());
        } finally { sender.stop(); }
    }
    @Test public void rejectedHandshakeRetriesKeepTheFailureVisible() throws Exception {
        try (ServerSocket receiver = new ServerSocket(0)) {
            receiver.setSoTimeout(5000);
            java.util.List<String> events = new java.util.concurrent.CopyOnWriteArrayList<>();
            NetworkStreamer sender = sender(receiver.getLocalPort(), events::add);
            sender.start();
            try {
                for (int attempt = 0; attempt < 3; ++attempt) {
                    try (Socket rejected = receiver.accept()) {
                        rejected.setSoTimeout(3000);
                        DataInputStream input = new DataInputStream(rejected.getInputStream());
                        StreamProtocol.Header hello = StreamProtocol.readHeader(input);
                        assertEquals(StreamProtocol.TYPE_HELLO, hello.type);
                        input.readFully(new byte[hello.payloadSize]);
                        // Existing receiver rejects pairing by closing without a PONG.
                    }
                    await(() -> sender.connectionMessage().equals(ConnectionFeedback.CLOSED));
                }
                int firstFailure = events.indexOf(ConnectionFeedback.CLOSED);
                assertTrue(firstFailure >= 0);
                for (String event : events.subList(firstFailure, events.size())) {
                    assertFalse(event.equals(ConnectionFeedback.CONNECTING));
                    assertFalse(event.equals(ConnectionFeedback.WAITING));
                }
                assertFalse(sender.receiverResponsive());
            } finally { sender.stop(); }
        }
    }
    @Test public void unconfirmedConnectionTimesOutAndRetries() throws Exception {
        try (ServerSocket receiver = new ServerSocket(0)) {
            java.util.concurrent.CountDownLatch timeout = new java.util.concurrent.CountDownLatch(1);
            NetworkStreamer sender = sender(receiver.getLocalPort(), message -> {
                if (message.contains("not responding")) timeout.countDown();
            });
            sender.start();
            try (Socket connection = receiver.accept()) {
                connection.setSoTimeout(3000); ping(connection);
                assertTrue(sender.connectionMessage(), timeout.await(13, java.util.concurrent.TimeUnit.SECONDS));
                assertFalse(sender.receiverResponsive());
            } finally { sender.stop(); }
        }
    }
}
