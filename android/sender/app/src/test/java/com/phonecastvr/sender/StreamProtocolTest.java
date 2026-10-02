package com.phonecastvr.sender;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;

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

    @Test public void validatesPairCodes() {
        assertTrue(StreamProtocol.validPairCode("123456"));
        assertFalse(StreamProtocol.validPairCode("12345"));
        assertFalse(StreamProtocol.validPairCode("12345x"));
    }
}
