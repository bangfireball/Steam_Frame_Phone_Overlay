package com.phonecastvr.sender;

final class CaptureConfig {
    static final int I_FRAME_INTERVAL_SECONDS = 2;

    enum StreamProfile {
        BATTERY_SAVER("Battery Saver", 960, 20, 1_000_000, 4_000_000, 8),
        STANDARD("Standard", 1280, 30, 2_000_000, 10_000_000, 10),
        QUALITY("Quality", 1920, 30, 4_000_000, 16_000_000, 9);

        final String label;
        final int maxLongEdge;
        final int frameRate;
        final int minimumBitrate;
        final int maximumBitrate;
        final int bitrateDivisor;

        StreamProfile(String label, int maxLongEdge, int frameRate,
                      int minimumBitrate, int maximumBitrate, int bitrateDivisor) {
            this.label = label;
            this.maxLongEdge = maxLongEdge;
            this.frameRate = frameRate;
            this.minimumBitrate = minimumBitrate;
            this.maximumBitrate = maximumBitrate;
            this.bitrateDivisor = bitrateDivisor;
        }

        static StreamProfile fromStoredName(String name) {
            if (name != null) {
                try {
                    return valueOf(name);
                } catch (IllegalArgumentException ignored) {
                    // A renamed or corrupt preference safely falls back to Standard.
                }
            }
            return STANDARD;
        }

        @Override public String toString() {
            return label;
        }
    }

    private CaptureConfig() {}

    static Size fitWithinLimit(int width, int height, StreamProfile profile) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("Capture dimensions must be positive");
        }
        if (profile == null) profile = StreamProfile.STANDARD;
        double scale = Math.min(1.0, (double) profile.maxLongEdge / Math.max(width, height));
        int scaledWidth = makeEven((int) Math.round(width * scale));
        int scaledHeight = makeEven((int) Math.round(height * scale));
        return new Size(Math.max(2, scaledWidth), Math.max(2, scaledHeight));
    }

    static int bitrateFor(Size size, StreamProfile profile) {
        if (profile == null) profile = StreamProfile.STANDARD;
        long bitrate = (long) size.width * size.height * profile.frameRate /
                profile.bitrateDivisor;
        return (int) Math.max(profile.minimumBitrate,
                Math.min(profile.maximumBitrate, bitrate));
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
