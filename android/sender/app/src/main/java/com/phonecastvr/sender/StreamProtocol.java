package com.phonecastvr.sender;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

final class StreamProtocol {
    static final int VERSION = 1;
    static final int DEFAULT_PORT = 49321;
    static final int TYPE_HELLO = 1;
    static final int TYPE_VIDEO_CONFIG = 2;
    static final int TYPE_VIDEO_FRAME = 3;
    static final int TYPE_PING = 4;
    static final int TYPE_PONG = 5;
    static final int TYPE_END_STREAM = 6;
    static final int TYPE_REQUEST_KEY_FRAME = 7;
    static final int TYPE_REMOTE_INPUT = 8;
    static final int FLAG_KEY_FRAME = 1;
    static final int MAX_PAYLOAD_SIZE = 4 * 1024 * 1024;

    private StreamProtocol() {}

    static void write(DataOutputStream output, int type, int flags, long sequence,
                      long timestampMicros, int width, int height, byte[] payload)
            throws IOException {
        if (payload == null) payload = new byte[0];
        if (payload.length > MAX_PAYLOAD_SIZE) {
            throw new IOException("Protocol payload exceeds 4 MiB");
        }
        output.writeByte('P');
        output.writeByte('C');
        output.writeByte('V');
        output.writeByte('R');
        output.writeByte(VERSION);
        output.writeByte(type);
        output.writeShort(flags);
        output.writeInt(payload.length);
        output.writeLong(sequence);
        output.writeLong(timestampMicros);
        output.writeShort(width);
        output.writeShort(height);
        output.write(payload);
    }

    static Header readHeader(DataInputStream input) throws IOException {
        if (input.readUnsignedByte() != 'P' || input.readUnsignedByte() != 'C' ||
                input.readUnsignedByte() != 'V' || input.readUnsignedByte() != 'R') {
            throw new IOException("Invalid PhoneCast protocol magic");
        }
        int version = input.readUnsignedByte();
        if (version != VERSION) throw new IOException("Unsupported PhoneCast protocol version");
        int type = input.readUnsignedByte();
        int flags = input.readUnsignedShort();
        int payloadSize = input.readInt();
        if (payloadSize < 0 || payloadSize > MAX_PAYLOAD_SIZE) {
            throw new IOException("Invalid PhoneCast payload size");
        }
        long sequence = input.readLong();
        long timestampMicros = input.readLong();
        int width = input.readUnsignedShort();
        int height = input.readUnsignedShort();
        return new Header(type, flags, payloadSize, sequence, timestampMicros, width, height);
    }

    static RemoteInputEvent parseRemoteInput(byte[] payload, long sequence) throws IOException {
        if (payload == null || payload.length != 16) {
            throw new IOException("Remote-input payload must be exactly 16 bytes");
        }
        DataInputStream input = new DataInputStream(new ByteArrayInputStream(payload));
        int type = input.readUnsignedByte();
        input.skipBytes(3);
        float x = input.readFloat();
        float y = input.readFloat();
        float scroll = input.readFloat();
        if (type < RemoteInputEvent.DOWN || type > RemoteInputEvent.BACK ||
                !Float.isFinite(x) || !Float.isFinite(y) || !Float.isFinite(scroll) ||
                x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f ||
                scroll < -1.0f || scroll > 1.0f) {
            throw new IOException("Invalid normalized remote-input event");
        }
        return new RemoteInputEvent(type, x, y, scroll, sequence);
    }

    static boolean validPairCode(String value) {
        return value != null && value.matches("[0-9]{6}");
    }

    static final class Header {
        final int type;
        final int flags;
        final int payloadSize;
        final long sequence;
        final long timestampMicros;
        final int width;
        final int height;

        Header(int type, int flags, int payloadSize, long sequence, long timestampMicros,
               int width, int height) {
            this.type = type;
            this.flags = flags;
            this.payloadSize = payloadSize;
            this.sequence = sequence;
            this.timestampMicros = timestampMicros;
            this.width = width;
            this.height = height;
        }
    }
}
