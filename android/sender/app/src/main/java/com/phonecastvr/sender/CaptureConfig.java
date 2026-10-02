package com.phonecastvr.sender;

final class CaptureConfig {
    static final int FRAME_RATE = 30;
    static final int I_FRAME_INTERVAL_SECONDS = 1;
    static final int MAX_LONG_EDGE = 1920;

    private CaptureConfig() {}

    static Size fitWithinLimit(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("Capture dimensions must be positive");
        }
        double scale = Math.min(1.0, (double) MAX_LONG_EDGE / Math.max(width, height));
        int scaledWidth = makeEven((int) Math.round(width * scale));
        int scaledHeight = makeEven((int) Math.round(height * scale));
        return new Size(Math.max(2, scaledWidth), Math.max(2, scaledHeight));
    }

    static int bitrateFor(Size size) {
        // About 0.10 bits per pixel per frame, bounded for mobile hardware encoders.
        long bitrate = (long) size.width * size.height * FRAME_RATE / 10L;
        return (int) Math.max(2_000_000L, Math.min(10_000_000L, bitrate));
    }

    private static int makeEven(int value) {
        return value & ~1;
    }

    static final class Size {
        final int width;
        final int height;

        Size(int width, int height) {
            this.width = width;
            this.height = height;
        }

        @Override public boolean equals(Object other) {
            if (!(other instanceof Size)) return false;
            Size size = (Size) other;
            return width == size.width && height == size.height;
        }

        @Override public int hashCode() {
            return 31 * width + height;
        }
    }
}
