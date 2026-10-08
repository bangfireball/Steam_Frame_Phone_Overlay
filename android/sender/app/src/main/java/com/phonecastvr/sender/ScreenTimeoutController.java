package com.phonecastvr.sender;

/** Casting-only timeout ownership; never changes keyguard or wakes a locked device. */
final class ScreenTimeoutController {
    static final int CASTING_TIMEOUT_MS = Integer.MAX_VALUE;

    interface Access {
        boolean canWrite();
        int read();
        boolean write(int value);
    }
    interface Recovery {
        boolean active();
        int saved();
        boolean save(int value);
        boolean clear();
    }
    private final Access access;
    private final Recovery recovery;

    ScreenTimeoutController(Access access, Recovery recovery) {
        this.access = access;
        this.recovery = recovery;
    }

    boolean begin() {
        // Resolve abandoned ownership before taking a new baseline.
        if (recovery.active() && !restore()) return false;
        if (!access.canWrite()) return false;
        int original = access.read();
        if (original < 0 || !recovery.save(original)) return false;
        if (!access.write(CASTING_TIMEOUT_MS)) return false;
        // Do not claim success when an OEM rejects/clamps the setting.
        if (access.read() != CASTING_TIMEOUT_MS) {
            restore();
            return false;
        }
        return true;
    }

    boolean restore() {
        if (!recovery.active()) return true;
        if (!access.canWrite()) return false;
        int current = access.read();
        if (current < 0) return false;
        // Preserve deliberate user changes (or an OEM's replacement value).
        if (current == CASTING_TIMEOUT_MS &&
                (!access.write(recovery.saved()) || access.read() != recovery.saved()))
            return false;
        return recovery.clear();
    }
}
