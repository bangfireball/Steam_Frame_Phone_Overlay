package com.phonecastvr.sender;

import org.junit.Test;
import static org.junit.Assert.*;

public class ScreenBrightnessControllerTest {
    private static final class Access implements ScreenBrightnessController.BrightnessAccess {
        boolean permitted = true;
        boolean failBrightness;
        boolean failMode;
        int brightness = 180;
        int mode = 1;
        @Override public boolean canWrite() { return permitted; }
        @Override public int brightness() { return brightness; }
        @Override public int mode() { return mode; }
        @Override public boolean writeBrightness(int value) {
            if (failBrightness) return false;
            brightness = value;
            return true;
        }
        @Override public boolean writeMode(int value) {
            if (failMode) return false;
            mode = value;
            return true;
        }
    }

    private static final class State implements ScreenBrightnessController.RecoveryState {
        boolean active;
        boolean failSave;
        int savedBrightness;
        int savedMode;
        int expectedBrightness;
        int expectedMode;
        int saves;
        @Override public boolean active() { return active; }
        @Override public int savedBrightness() { return savedBrightness; }
        @Override public int savedMode() { return savedMode; }
        @Override public int expectedBrightness() { return expectedBrightness; }
        @Override public int expectedMode() { return expectedMode; }
        @Override public boolean save(int brightness, int mode, int expectedBrightness,
                                      int expectedMode) {
            if (failSave) return false;
            active = true;
            savedBrightness = brightness;
            savedMode = mode;
            this.expectedBrightness = expectedBrightness;
            this.expectedMode = expectedMode;
            ++saves;
            return true;
        }
        @Override public boolean updateExpected(int brightness, int mode) {
            if (failSave) return false;
            expectedBrightness = brightness;
            expectedMode = mode;
            return true;
        }
        @Override public void clear() { active = false; }
    }

    @Test public void dimDefaultsToSafeNonzeroLevelAndRestoresAdaptiveMode() {
        Access access = new Access(); State state = new State();
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertTrue(controller.dim());
        assertEquals(1, access.brightness);
        assertEquals(ScreenBrightnessController.MODE_MANUAL, access.mode);
        assertTrue(state.active);
        assertEquals(180, state.savedBrightness);
        assertEquals(1, state.savedMode);
        assertTrue(controller.restoreAndRelease());
        assertEquals(180, access.brightness);
        assertEquals(1, access.mode);
        assertFalse(state.active);
    }

    @Test public void repeatedDimDoesNotOverwriteOriginalSettings() {
        Access access = new Access(); State state = new State();
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertTrue(controller.dim());
        assertTrue(controller.dim());
        assertEquals(1, state.saves);
        assertEquals(180, state.savedBrightness);
    }

    @Test public void temporaryRestoreKeepsRecoveryMarkerForRedim() {
        Access access = new Access(); State state = new State();
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertTrue(controller.dim());
        assertTrue(controller.restoreTemporarily());
        assertEquals(180, access.brightness);
        assertEquals(1, access.mode);
        assertTrue(state.active);
        assertTrue(controller.dim());
        assertEquals(1, access.brightness);
        assertEquals(1, state.saves);
    }

    @Test public void deniedCapabilityNeverSavesOrChangesSettings() {
        Access access = new Access(); access.permitted = false;
        State state = new State();
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertFalse(controller.dim());
        assertFalse(state.active);
        assertEquals(180, access.brightness);
    }

    @Test public void userBrightnessChangeIsPreservedWhileOriginalModeReturns() {
        Access access = new Access(); State state = new State();
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertTrue(controller.dim());
        access.brightness = 90;
        assertTrue(controller.restoreAndRelease());
        assertEquals(90, access.brightness);
        assertEquals(1, access.mode);
        assertFalse(state.active);
    }

    @Test public void failedRestoreRetainsDurableRecoveryState() {
        Access access = new Access(); State state = new State();
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertTrue(controller.dim());
        access.failMode = true;
        assertFalse(controller.restoreAndRelease());
        assertTrue(state.active);
        access.failMode = false;
        assertTrue(controller.recoverAbandonedOverride());
        assertFalse(state.active);
    }

    @Test public void abandonedOverrideRecoversOnNextController() {
        Access access = new Access(); access.brightness = 1; access.mode = 0;
        State state = new State(); state.active = true;
        state.savedBrightness = 140; state.savedMode = 1;
        state.expectedBrightness = 1; state.expectedMode = 0;
        ScreenBrightnessController controller = new ScreenBrightnessController(access, state, 1);
        assertTrue(controller.recoverAbandonedOverride());
        assertEquals(140, access.brightness);
        assertEquals(1, access.mode);
        assertFalse(state.active);
    }
}
