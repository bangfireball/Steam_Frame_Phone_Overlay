package com.phonecastvr.sender;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.projection.MediaProjection;
import android.util.Log;

import java.util.concurrent.atomic.AtomicBoolean;

/** Playback-only capture: never supplies a microphone source or requests audio focus. */
final class PlaybackAudioCapture {
    static final String PREFERENCE_ENABLED = "playback_audio_enabled";
    interface Listener {
        void onStatus(int state, String message);
    }
    private final Context context;
    private final MediaProjection projection;
    private final NetworkStreamer streamer;
    private final Listener listener;
    private final long epoch = System.nanoTime();
    private final AtomicBoolean running = new AtomicBoolean();
    private volatile AudioRecord recorder;

    PlaybackAudioCapture(Context context, MediaProjection projection, NetworkStreamer streamer,
                         Listener listener) {
        this.context = context; this.projection = projection; this.streamer = streamer; this.listener = listener;
    }
    void start() {
        if (!running.compareAndSet(false,true)) return;
        streamer.beginAudio(epoch);
        new Thread(this::capture,"phonecast-playback-capture").start();
    }
    void stop() {
        running.set(false);
        AudioRecord active = recorder;
        if (active != null) {
            try { active.stop(); } catch (IllegalStateException ignored) { /* Already released. */ }
        }
        streamer.endAudio(epoch,AudioProtocol.OFF);
    }
    private boolean allowed() {
        return running.get() && streamer.audioCapable() &&
                context.checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED &&
                context.getSharedPreferences(ScreenCaptureService.PREFERENCES,Context.MODE_PRIVATE)
                        .getBoolean(PREFERENCE_ENABLED,false);
    }
    private void capture() {
        AudioRecord active = null;
        boolean failed = false;
        try {
            if (!allowed()) return;
            AudioPlaybackCaptureConfiguration config = new AudioPlaybackCaptureConfiguration.Builder(projection)
                    .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                    .addMatchingUsage(AudioAttributes.USAGE_GAME)
                    .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN).build();
            AudioFormat format = new AudioFormat.Builder().setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setSampleRate(AudioProtocol.SAMPLE_RATE).setChannelMask(AudioFormat.CHANNEL_IN_STEREO).build();
            int minimum = AudioRecord.getMinBufferSize(AudioProtocol.SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_STEREO,AudioFormat.ENCODING_PCM_16BIT);
            if (minimum <= 0) throw new IllegalStateException("48 kHz stereo capture is unavailable");
            if (context.checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
                throw new SecurityException("Playback audio permission revoked");
            active = new AudioRecord.Builder().setAudioFormat(format)
                    .setBufferSizeInBytes(Math.max(minimum,AudioProtocol.BYTES * 4))
                    .setAudioPlaybackCaptureConfig(config).build();
            recorder = active;
            if (active.getState() != AudioRecord.STATE_INITIALIZED || active.getSampleRate() != AudioProtocol.SAMPLE_RATE ||
                    active.getChannelCount() != AudioProtocol.CHANNELS)
                throw new IllegalStateException("Playback recorder format unavailable");
            if (!allowed()) return;
            active.startRecording();
            if (active.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING)
                throw new IllegalStateException("Playback recorder did not start");
            listener.onStatus(AudioProtocol.ACTIVE,"Playback audio capturing");
            streamer.audioStatus(epoch,AudioProtocol.ACTIVE);
            short[] samples = new short[AudioProtocol.FRAMES * AudioProtocol.CHANNELS];
            AudioTimestamp timestamp = new AudioTimestamp();
            long sampleFrames = 0, sequence = 0, previousPts = 0;
            int fill = 0, silentBlocks = 0;
            boolean silent = false, fallbackReported = false;
            while (allowed()) {
                int count = active.read(samples,fill,samples.length - fill,AudioRecord.READ_BLOCKING);
                if (count <= 0) {
                    if (!allowed()) break;
                    throw new IllegalStateException("Playback recorder read failed: " + count);
                }
                fill += count;
                if (fill != samples.length) continue;
                long pts;
                if (active.getTimestamp(timestamp,AudioTimestamp.TIMEBASE_MONOTONIC) == AudioRecord.SUCCESS) {
                    pts = AudioProtocol.timestampMicros(sampleFrames,timestamp.framePosition,timestamp.nanoTime);
                } else {
                    pts = System.nanoTime() / 1000L - 10000L;
                    if (!fallbackReported) {
                        Log.w("PhoneCastAudio","Recorder timestamps unavailable; using approximate read-time anchor");
                        fallbackReported = true;
                    }
                }
                if (previousPts > 0 && pts <= previousPts) pts = previousPts + 10000L;
                previousPts = pts;
                boolean zero = true;
                for (short value : samples) if (value != 0) { zero = false; break; }
                silentBlocks = zero ? silentBlocks + 1 : 0;
                boolean nowSilent = silentBlocks >= 200;
                if (nowSilent != silent) {
                    silent = nowSilent;
                    int state = silent ? AudioProtocol.SILENT : AudioProtocol.ACTIVE;
                    streamer.audioStatus(epoch,state);
                    listener.onStatus(state,silent ? "No capturable playback; source may be silent or disallow capture"
                            : "Playback audio capturing");
                }
                streamer.offerAudio(epoch,sequence++,pts,AudioProtocol.block(epoch,samples));
                sampleFrames += AudioProtocol.FRAMES; fill = 0;
            }
            if (running.get()) listener.onStatus(AudioProtocol.OFF,"Playback audio consent/permission ended; video continues");
        } catch (RuntimeException error) {
            if (running.get()) {
                failed = true;
                Log.w("PhoneCastAudio","Playback capture failed: " + error.getClass().getSimpleName());
                listener.onStatus(AudioProtocol.ERROR,"Playback audio unavailable: " +
                        (error.getMessage() == null ? error.getClass().getSimpleName() : error.getMessage()));
            }
        } finally {
            running.set(false); recorder = null;
            if (active != null) {
                try { active.stop(); } catch (IllegalStateException ignored) { /* Not started. */ }
                active.release();
            }
            streamer.endAudio(epoch,failed ? AudioProtocol.ERROR : AudioProtocol.OFF);
        }
    }
}
