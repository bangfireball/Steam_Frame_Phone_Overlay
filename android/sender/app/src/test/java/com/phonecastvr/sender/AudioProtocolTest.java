package com.phonecastvr.sender;

import org.junit.Test;
import java.util.Arrays;
import static org.junit.Assert.*;

public class AudioProtocolTest {
    @Test public void capabilityRequiresExactSupportedVersion() {
        assertTrue(AudioProtocol.hasCapability(new byte[]{'P','C','A','C',1,1,0,0}));
        assertFalse(AudioProtocol.hasCapability(new byte[0]));
        byte[] changed = AudioProtocol.capability(); changed[4] = 2;
        assertFalse(AudioProtocol.hasCapability(changed));
        changed = AudioProtocol.capability(); changed[7] = 1;
        assertFalse(AudioProtocol.hasCapability(changed));
    }
    @Test public void configMatchesCppGoldenWire() {
        assertArrayEquals(new byte[]{1,1,2,0,0,0,(byte)0xbb,(byte)0x80,1,2,3,4,5,6,7,8},
                AudioProtocol.config(0x0102030405060708L));
    }
    @Test public void pcmSamplesAreLittleEndianButMetadataIsBigEndian() {
        short[] samples = new short[960]; samples[0] = 0x1234; samples[1] = -32768; samples[2] = -1;
        byte[] payload = AudioProtocol.block(0x0102030405060708L,samples);
        assertEquals(1932,payload.length);
        assertArrayEquals(new byte[]{1,2,3,4,5,6,7,8,1,(byte)0xe0,0,0,0x34,0x12,0,(byte)0x80,(byte)0xff,(byte)0xff},
                Arrays.copyOf(payload,18));
    }
    @Test public void statusAndTimestampsHaveExplicitEpochAndUnits() {
        assertArrayEquals(new byte[]{1,2,0,0,0,0,0,0,0,0,0,9},AudioProtocol.status(9,AudioProtocol.SILENT));
        assertEquals(990000L,AudioProtocol.timestampMicros(0,480,1_000_000_000L));
        assertEquals(1_010_000L,AudioProtocol.timestampMicros(960,480,1_000_000_000L));
    }
    @Test public void rejectInvalidFormatOrEpoch() {
        try { AudioProtocol.config(0); fail(); } catch (IllegalArgumentException expected) { /* Correct. */ }
        try { AudioProtocol.block(1,new short[959]); fail(); } catch (IllegalArgumentException expected) { /* Correct. */ }
        try { AudioProtocol.status(1,4); fail(); } catch (IllegalArgumentException expected) { /* Correct. */ }
    }
}
