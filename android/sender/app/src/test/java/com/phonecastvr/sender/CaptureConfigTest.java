package com.phonecastvr.sender;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class CaptureConfigTest {
    @Test public void nativeSmallResolutionIsPreservedAndEven() {
        CaptureConfig.Size size = CaptureConfig.fitWithinLimit(1080, 2340);
        assertEquals(886, size.width);
        assertEquals(1920, size.height);
        assertEquals(0, size.width % 2);
        assertEquals(0, size.height % 2);
    }

    @Test public void landscapeOrientationIsPreserved() {
        CaptureConfig.Size size = CaptureConfig.fitWithinLimit(2340, 1080);
        assertEquals(1920, size.width);
        assertEquals(886, size.height);
    }

    @Test public void lowResolutionIsNotUpscaled() {
        assertEquals(new CaptureConfig.Size(720, 1280), CaptureConfig.fitWithinLimit(720, 1280));
    }

    @Test public void bitrateIsBounded() {
        assertEquals(2_000_000, CaptureConfig.bitrateFor(new CaptureConfig.Size(640, 360)));
        assertTrue(CaptureConfig.bitrateFor(new CaptureConfig.Size(1920, 1080)) <= 10_000_000);
    }
}
