package com.phonecastvr.sender;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;

final class AudioProtocol {
    static final int CONFIG = 12, FRAME = 13, STATUS = 14;
    static final int SAMPLE_RATE = 48000, CHANNELS = 2, FRAMES = 480, BYTES = 1920;
    static final int OFF = 0, ACTIVE = 1, SILENT = 2, ERROR = 3;
    private static final byte[] CAPABILITY = {'P','C','A','C',1,1,0,0};
    private AudioProtocol() {}
    static byte[] capability() { return CAPABILITY.clone(); }
    static boolean hasCapability(byte[] payload) { return Arrays.equals(payload,CAPABILITY); }
    static byte[] config(long epoch) {
        if (epoch <= 0) throw new IllegalArgumentException("Invalid audio epoch");
        return ByteBuffer.allocate(16).order(ByteOrder.BIG_ENDIAN)
                .put((byte)1).put((byte)1).put((byte)2).put((byte)0)
                .putInt(SAMPLE_RATE).putLong(epoch).array();
    }
    static byte[] block(long epoch, short[] samples) {
        if (epoch <= 0 || samples == null || samples.length != FRAMES * CHANNELS)
            throw new IllegalArgumentException("Invalid PCM block");
        ByteBuffer buffer = ByteBuffer.allocate(12 + BYTES).order(ByteOrder.BIG_ENDIAN);
        buffer.putLong(epoch).putShort((short)FRAMES).putShort((short)0);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        for (short sample : samples) buffer.putShort(sample);
        return buffer.array();
    }
    static byte[] status(long epoch, int state) {
        if (epoch <= 0 || state < OFF || state > ERROR) throw new IllegalArgumentException("Invalid audio status");
        return ByteBuffer.allocate(12).order(ByteOrder.BIG_ENDIAN)
                .put((byte)1).put((byte)state).putShort((short)0).putLong(epoch).array();
    }
    static long timestampMicros(long firstFrame, long observedFrame, long observedNanos) {
        return observedNanos / 1000L + (firstFrame - observedFrame) * 1_000_000L / SAMPLE_RATE;
    }
}
