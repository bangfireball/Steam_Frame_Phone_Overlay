package com.phonecastvr.sender;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioManager;
import android.util.Log;

/** Android adapter for the testable phone-volume mute lifecycle. */
final class AndroidPhoneAudioMute {
    static final String PREFERENCE_ENABLED = "mute_phone_audio_while_streaming";
    private static final String PREFERENCE_RECOVERY_ACTIVE = "phone_audio_mute_recovery_active";
    private static final String PREFERENCE_RECOVERY_VOLUME = "phone_audio_mute_recovery_volume";
    private static final String TAG = "PhoneCastAudio";

    static PhoneAudioMuteController create(Context context) {
        Context app = context.getApplicationContext();
        AudioManager audio = (AudioManager) app.getSystemService(Context.AUDIO_SERVICE);
        SharedPreferences preferences = app.getSharedPreferences(
                ScreenCaptureService.PREFERENCES,Context.MODE_PRIVATE);
        return new PhoneAudioMuteController(new PhoneAudioMuteController.VolumeAccess() {
            @Override public int currentVolume() {
                return audio.getStreamVolume(AudioManager.STREAM_MUSIC);
            }

            @Override public boolean setVolume(int volume) {
                try {
                    int bounded = Math.max(0,Math.min(volume,
                            audio.getStreamMaxVolume(AudioManager.STREAM_MUSIC)));
                    audio.setStreamVolume(AudioManager.STREAM_MUSIC,bounded,0);
                    return audio.getStreamVolume(AudioManager.STREAM_MUSIC) == bounded;
                } catch (RuntimeException error) {
                    Log.w(TAG,"Could not change local media volume: " +
                            error.getClass().getSimpleName());
                    return false;
                }
            }
        },new PhoneAudioMuteController.RecoveryState() {
            @Override public boolean hasSavedVolume() {
                return preferences.getBoolean(PREFERENCE_RECOVERY_ACTIVE,false);
            }

            @Override public int savedVolume() {
                return preferences.getInt(PREFERENCE_RECOVERY_VOLUME,0);
            }

            @Override public boolean saveBeforeMute(int volume) {
                return preferences.edit()
                        .putInt(PREFERENCE_RECOVERY_VOLUME,volume)
                        .putBoolean(PREFERENCE_RECOVERY_ACTIVE,true)
                        .commit();
            }

            @Override public void clear() {
                preferences.edit()
                        .remove(PREFERENCE_RECOVERY_VOLUME)
                        .remove(PREFERENCE_RECOVERY_ACTIVE)
                        .commit();
            }
        });
    }

    static void recoverAfterUncleanShutdown(Context context) {
        if (!create(context).recoverAbandonedMute())
            Log.w(TAG,"Could not restore media volume left muted by an interrupted cast");
    }

    private AndroidPhoneAudioMute() {}
}
