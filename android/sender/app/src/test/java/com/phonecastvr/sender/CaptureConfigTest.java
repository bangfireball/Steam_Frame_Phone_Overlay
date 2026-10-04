package com.phonecastvr.sender;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class CaptureConfigTest {
    @Test public void standardProfilePreservesAspectRatioAndEvenDimensions() {
        CaptureConfig.Size size = CaptureConfig.fitWithinLimit(1080, 2340,
                CaptureConfig.StreamProfile.STANDARD);
        assertEquals(590, size.width);
        assertEquals(1280, size.height);
        assertEquals(0, size.width % 2);
        assertEquals(0, size.height % 2);
    }

    @Test public void landscapeOrientationIsPreserved() {
        CaptureConfig.Size size = CaptureConfig.fitWithinLimit(2340, 1080,
                CaptureConfig.StreamProfile.STANDARD);
        assertEquals(1280, size.width);
        assertEquals(590, size.height);
    }

    @Test public void lowResolutionIsNotUpscaled() {
        assertEquals(new CaptureConfig.Size(720, 1280), CaptureConfig.fitWithinLimit(
                720, 1280, CaptureConfig.StreamProfile.STANDARD));
    }

    @Test public void profilesApplyDistinctLimitsAndFrameRates() {
        assertEquals(new CaptureConfig.Size(442, 960), CaptureConfig.fitWithinLimit(
                1080, 2340, CaptureConfig.StreamProfile.BATTERY_SAVER));
        assertEquals(new CaptureConfig.Size(886, 1920), CaptureConfig.fitWithinLimit(
                1080, 2340, CaptureConfig.StreamProfile.QUALITY));
        assertEquals(20, CaptureConfig.StreamProfile.BATTERY_SAVER.frameRate);
        assertEquals(30, CaptureConfig.StreamProfile.STANDARD.frameRate);
    }

    @Test public void bitrateIsBoundedPerProfile() {
        assertEquals(1_000_000, CaptureConfig.bitrateFor(new CaptureConfig.Size(640, 360),
                CaptureConfig.StreamProfile.BATTERY_SAVER));
        assertTrue(CaptureConfig.bitrateFor(new CaptureConfig.Size(1920, 1080),
                CaptureConfig.StreamProfile.QUALITY) <= 16_000_000);
    }

    @Test public void invalidStoredProfileFallsBackToStandard() {
        assertEquals(CaptureConfig.StreamProfile.STANDARD,
                CaptureConfig.StreamProfile.fromStoredName("missing"));
        assertEquals(CaptureConfig.StreamProfile.QUALITY,
                CaptureConfig.StreamProfile.fromStoredName("QUALITY"));
    }
}
