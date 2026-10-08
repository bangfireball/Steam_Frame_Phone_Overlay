package com.phonecastvr.sender;

import org.junit.Test;
import static org.junit.Assert.*;

public class ScreenTimeoutControllerTest {
    private static final class Fixture implements ScreenTimeoutController.Access, ScreenTimeoutController.Recovery {
        int value = 15000, original;
        boolean allowed = true, active, saveOk = true, writeOk = true, clearOk = true, clamp;
        int writes;
        public boolean canWrite() { return allowed; }
        public int read() { return value; }
        public boolean write(int next) { writes++; if (!writeOk) return false; value = clamp ? 600000 : next; return true; }
        public boolean active() { return active; }
        public int saved() { return original; }
        public boolean save(int next) { if (!saveOk) return false; original = next; active = true; return true; }
        public boolean clear() { if (!clearOk) return false; active = false; return true; }
        ScreenTimeoutController controller() { return new ScreenTimeoutController(this, this); }
    }
    @Test public void extendsAndRestores() {
        Fixture f = new Fixture(); ScreenTimeoutController c = f.controller();
        assertTrue(c.begin()); assertEquals(Integer.MAX_VALUE, f.value);
        assertEquals(15000, f.original); assertTrue(c.restore());
        assertEquals(15000, f.value); assertFalse(f.active); assertTrue(c.restore());
    }
    @Test public void abandonedSessionRestoresBeforeNewBaseline() {
        Fixture f = new Fixture(); assertTrue(f.controller().begin());
        assertTrue(f.controller().begin()); assertEquals(15000, f.original);
        assertTrue(f.controller().restore()); assertEquals(15000, f.value);
    }
    @Test public void deniedPermissionAndFailedPersistenceNeverWrite() {
        Fixture f = new Fixture(); f.allowed = false; assertFalse(f.controller().begin());
        f.allowed = true; f.saveOk = false; assertFalse(f.controller().begin()); assertEquals(0, f.writes);
    }
    @Test public void preservesUserChange() {
        Fixture f = new Fixture(); assertTrue(f.controller().begin()); f.value = 60000;
        assertTrue(f.controller().restore()); assertEquals(60000, f.value);
    }
    @Test public void revokedAccessRetainsRecovery() {
        Fixture f = new Fixture(); assertTrue(f.controller().begin()); f.allowed = false;
        assertFalse(f.controller().restore()); assertTrue(f.active);
        f.allowed = true; assertTrue(f.controller().restore()); assertEquals(15000, f.value);
    }
    @Test public void failedRestoreRetainsRecovery() {
        Fixture f = new Fixture(); assertTrue(f.controller().begin()); f.writeOk = false;
        assertFalse(f.controller().restore()); assertTrue(f.active);
        f.writeOk = true; assertTrue(f.controller().restore());
    }
    @Test public void failedInitialWriteAndClampingDoNotClaimProtection() {
        Fixture f = new Fixture(); f.writeOk = false; assertFalse(f.controller().begin());
        f.writeOk = true; assertTrue(f.controller().restore());
        f.clamp = true; assertFalse(f.controller().begin()); assertFalse(f.active);
    }
    @Test public void failedRecoveryClearAndReadRetainOwnership() {
        Fixture f = new Fixture(); assertTrue(f.controller().begin()); f.value = -1;
        assertFalse(f.controller().restore()); assertTrue(f.active);
        f.value = Integer.MAX_VALUE; f.clearOk = false;
        assertFalse(f.controller().restore()); assertTrue(f.active);
        f.clearOk = true; assertTrue(f.controller().restore());
    }
}
