package com.phonecastvr.sender;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.ByteArrayInputStream;

public final class StreamProtocolTest {
    @Test public void writesVersionedBigEndianHeader() throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        StreamProtocol.write(new DataOutputStream(bytes), StreamProtocol.TYPE_VIDEO_FRAME,
                StreamProtocol.FLAG_KEY_FRAME, 42, 123456, 1080, 1920,
                new byte[]{1, 2, 3});
        byte[] message = bytes.toByteArray();
        assertEquals(35, message.length);
        assertEquals('P', message[0]);
        assertEquals('R', message[3]);
        assertEquals(StreamProtocol.VERSION, message[4]);
        assertEquals(StreamProtocol.TYPE_VIDEO_FRAME, message[5]);
        assertEquals(3, message[11]);
        assertEquals(1, message[32]);
    }

    @Test public void keyFrameRequestHasStableWireType() {
        assertEquals(7, StreamProtocol.TYPE_REQUEST_KEY_FRAME);
        assertEquals(8, StreamProtocol.TYPE_REMOTE_INPUT);
        assertEquals(9, StreamProtocol.TYPE_NOTIFICATION);
    }

    @Test public void writesBoundedNotificationPayload() throws Exception {
        byte[] payload = StreamProtocol.notificationPayload(
                "Messages", "Hello", "Are you coming?", "com.example.messages", 1234L, false);
        DataInputStream input = new DataInputStream(new ByteArrayInputStream(payload));
        assertEquals(1, input.readUnsignedByte());
        assertEquals(0, input.readUnsignedByte());
        assertEquals(8, input.readUnsignedShort());
        assertEquals(5, input.readUnsignedShort());
        assertEquals(15, input.readUnsignedShort());
        assertEquals(20, input.readUnsignedShort());
        assertEquals(1234L, input.readLong());
    }

    @Test public void notificationPolicyUsesAllowAndBlockLists() {
        assertTrue(NotificationPolicy.allows("com.example.chat", "", ""));
        assertTrue(NotificationPolicy.allows("com.example.chat",
                "com.example.chat, com.example.mail", ""));
        assertFalse(NotificationPolicy.allows("com.example.bank",
                "com.example.chat", ""));
        assertFalse(NotificationPolicy.allows("com.example.chat",
                "com.example.chat", "com.example.chat"));
        assertEquals("hello world", NotificationPolicy.clean(" hello\n world ", 20));
    }

    @Test public void parsesNormalizedRemoteInput() throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream output = new DataOutputStream(bytes);
        output.writeByte(RemoteInputEvent.SCROLL);
        output.write(new byte[3]);
        output.writeFloat(0.25f);
        output.writeFloat(0.75f);
        output.writeFloat(-0.5f);
        RemoteInputEvent event = StreamProtocol.parseRemoteInput(bytes.toByteArray(), 91);
        assertEquals(RemoteInputEvent.SCROLL, event.type);
        assertEquals(0.25f, event.normalizedX, 0.0001f);
        assertEquals(0.75f, event.normalizedY, 0.0001f);
        assertEquals(-0.5f, event.scrollDelta, 0.0001f);
        assertEquals(91, event.sequence);
    }

    @Test public void validatesPairCodes() {
        assertTrue(StreamProtocol.validPairCode("123456"));
        assertFalse(StreamProtocol.validPairCode("12345"));
        assertFalse(StreamProtocol.validPairCode("12345x"));
    }
}
