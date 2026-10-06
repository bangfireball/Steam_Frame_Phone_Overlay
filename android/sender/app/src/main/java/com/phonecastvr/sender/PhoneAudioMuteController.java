package com.phonecastvr.sender;

/**
 * Coordinates an opt-in local media-volume mute with streaming-audio lifecycle.
 * Platform access and durable state are injected so ordering and recovery remain testable.
 */
final class PhoneAudioMuteController {
    interface VolumeAccess {
        int currentVolume();
        boolean setVolume(int volume);
    }
    interface RecoveryState {
        boolean hasSavedVolume();
        int savedVolume();
        boolean saveBeforeMute(int volume);
        void clear();
    }

    private final VolumeAccess volume;
    private final RecoveryState recovery;
    private boolean requested;
    private boolean streamingAudio;
    private boolean muted;

    PhoneAudioMuteController(VolumeAccess volume, RecoveryState recovery) {
        this.volume = volume;
        this.recovery = recovery;
    }

    boolean recoverAbandonedMute() {
        if (!recovery.hasSavedVolume()) return true;
        int saved = recovery.savedVolume();
        if (!volume.setVolume(saved)) return false;
        recovery.clear();
        muted = false;
        return true;
    }

    void setRequested(boolean requested) {
        this.requested = requested;
        reconcile();
    }

    void setStreamingAudio(boolean streamingAudio) {
        this.streamingAudio = streamingAudio;
        reconcile();
    }

    boolean isMuted() { return muted; }

    private void reconcile() {
        if (requested && streamingAudio) mute();
        else restore();
    }

    private void mute() {
        if (muted) return;
        int saved = volume.currentVolume();
        // Commit recovery state before changing a global phone setting.
        if (!recovery.saveBeforeMute(saved)) return;
        if (!volume.setVolume(0)) {
            recovery.clear();
            return;
        }
        muted = true;
    }

    private void restore() {
        if (!muted && !recovery.hasSavedVolume()) return;
        int saved = recovery.savedVolume();
        if (!volume.setVolume(saved)) return; // Keep the marker for a later recovery attempt.
        recovery.clear();
        muted = false;
    }
}
