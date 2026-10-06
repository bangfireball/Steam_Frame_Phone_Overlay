package com.phonecastvr.sender;

import org.junit.Test;
import static org.junit.Assert.*;

public class PhoneAudioMuteControllerTest {
    private static final class Volume implements PhoneAudioMuteController.VolumeAccess {
        int value = 7;
        boolean fail;
        int writes;
        @Override public int currentVolume() { return value; }
        @Override public boolean setVolume(int value) {
            ++writes;
            if (fail) return false;
            this.value = value;
            return true;
        }
    }
    private static final class State implements PhoneAudioMuteController.RecoveryState {
        boolean active;
        int saved;
        int saves;
        @Override public boolean hasSavedVolume() { return active; }
        @Override public int savedVolume() { return saved; }
        @Override public boolean saveBeforeMute(int value) {
            active = true; saved = value; ++saves; return true;
        }
        @Override public void clear() { active = false; }
    }

    @Test public void muteIsOptInAndRestoresExactVolumeOnDisconnect() {
        Volume volume = new Volume(); State state = new State();
        PhoneAudioMuteController mute = new PhoneAudioMuteController(volume,state);
        mute.setStreamingAudio(true);
        assertEquals(7,volume.value);
        mute.setRequested(true);
        assertEquals(0,volume.value);
        assertTrue(mute.isMuted());
        assertTrue(state.active);
        mute.setStreamingAudio(false);
        assertEquals(7,volume.value);
        assertFalse(mute.isMuted());
        assertFalse(state.active);
    }

    @Test public void repeatedCallbacksDoNotOverwriteSavedVolume() {
        Volume volume = new Volume(); State state = new State();
        PhoneAudioMuteController mute = new PhoneAudioMuteController(volume,state);
        mute.setRequested(true); mute.setStreamingAudio(true);
        mute.setStreamingAudio(true); mute.setRequested(true);
        assertEquals(1,state.saves);
        mute.setRequested(false);
        assertEquals(7,volume.value);
    }

    @Test public void reconnectSavesTheThenCurrentVolume() {
        Volume volume = new Volume(); State state = new State();
        PhoneAudioMuteController mute = new PhoneAudioMuteController(volume,state);
        mute.setRequested(true); mute.setStreamingAudio(true);
        mute.setStreamingAudio(false);
        volume.value = 4;
        mute.setStreamingAudio(true);
        assertEquals(4,state.saved);
        mute.setStreamingAudio(false);
        assertEquals(4,volume.value);
    }

    @Test public void abandonedMuteRecoversOnNextProcessStart() {
        Volume volume = new Volume(); volume.value = 0;
        State state = new State(); state.active = true; state.saved = 9;
        PhoneAudioMuteController mute = new PhoneAudioMuteController(volume,state);
        assertTrue(mute.recoverAbandonedMute());
        assertEquals(9,volume.value);
        assertFalse(state.active);
    }

    @Test public void failedRestoreRetainsRecoveryMarker() {
        Volume volume = new Volume(); volume.value = 0; volume.fail = true;
        State state = new State(); state.active = true; state.saved = 6;
        PhoneAudioMuteController mute = new PhoneAudioMuteController(volume,state);
        assertFalse(mute.recoverAbandonedMute());
        assertTrue(state.active);
    }

    @Test public void failedMuteDoesNotLeaveFalseRecoveryState() {
        Volume volume = new Volume(); volume.fail = true;
        State state = new State();
        PhoneAudioMuteController mute = new PhoneAudioMuteController(volume,state);
        mute.setRequested(true); mute.setStreamingAudio(true);
        assertFalse(mute.isMuted());
        assertFalse(state.active);
        assertEquals(7,volume.value);
    }
}
