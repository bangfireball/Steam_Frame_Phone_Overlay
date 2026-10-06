package com.phonecastvr.sender;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public final class QuickConnectConfigTest {
    @Test public void savedReceiverAndPairCodeAreRequired() {
        assertTrue(QuickConnectConfig.isReady("10.0.0.42", "123456"));
        assertTrue(QuickConnectConfig.isReady(" frame.local ", "654321"));
        assertFalse(QuickConnectConfig.isReady("", "123456"));
        assertFalse(QuickConnectConfig.isReady("   ", "123456"));
        assertFalse(QuickConnectConfig.isReady("10.0.0.42", "12345"));
        assertFalse(QuickConnectConfig.isReady("10.0.0.42", null));
    }
}
