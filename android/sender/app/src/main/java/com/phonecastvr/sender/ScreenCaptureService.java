package com.phonecastvr.sender;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Surface;
import android.view.WindowManager;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicLong;

public final class ScreenCaptureService extends Service {
    static final String ACTION_START = "com.phonecastvr.sender.action.START";
    static final String ACTION_STOP = "com.phonecastvr.sender.action.STOP";
    static final String ACTION_STATUS = "com.phonecastvr.sender.action.STATUS";
    static final String EXTRA_RESULT_CODE = "result_code";
    static final String EXTRA_RESULT_DATA = "result_data";
    static final String EXTRA_RECEIVER_HOST = "receiver_host";
    static final String EXTRA_PAIR_CODE = "pair_code";
    static final String EXTRA_RUNNING = "running";
    static final String EXTRA_MESSAGE = "message";
    static final String EXTRA_FRAMES = "frames";
    static final String EXTRA_BYTES = "bytes";
    static final String EXTRA_WIDTH = "width";
    static final String EXTRA_HEIGHT = "height";
    static final String EXTRA_DROPPED_FRAMES = "dropped_frames";
    static final String EXTRA_NETWORK_RTT_MICROS = "network_rtt_micros";
    static final String PREFERENCES = "phonecast_sender";

    private static final String TAG = "PhoneCastCapture";
    private static final String CHANNEL_ID = "phonecast_capture";
    private static final int NOTIFICATION_ID = 100;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final Object encoderLock = new Object();
    private final AtomicLong encodedFrames = new AtomicLong();
    private final AtomicLong encodedBytes = new AtomicLong();

    private HandlerThread codecThread;
    private Handler codecHandler;
    private MediaProjection projection;
    private VirtualDisplay virtualDisplay;
    private EncoderSession encoderSession;
    private NetworkStreamer networkStreamer;
    private int densityDpi;
    private long lastStatusNanos;
    private boolean stopping;

    private final MediaProjection.Callback projectionCallback = new MediaProjection.Callback() {
        @Override public void onStop() {
            mainHandler.post(() -> stopCapture("Screen capture ended"));
        }

        @Override public void onCapturedContentResize(int width, int height) {
            Log.i(TAG, "Captured content resize callback: " + width + "x" + height);
            mainHandler.post(() -> reconfigureForSize(width, height));
        }
    };

    private final DisplayManager.DisplayListener displayListener =
            new DisplayManager.DisplayListener() {
                @Override public void onDisplayAdded(int displayId) {}
                @Override public void onDisplayRemoved(int displayId) {}

                @Override public void onDisplayChanged(int displayId) {
                    // onCapturedContentResize is authoritative on Android 14+, but this
                    // catches devices/codecs that omit that callback during rotation.
                    scheduleCurrentDisplaySizeCheck();
                }
            };

    @Override public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        codecThread = new HandlerThread("phonecast-avc-output");
        codecThread.start();
        codecHandler = new Handler(codecThread.getLooper());
        DisplayManager displayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
        displayManager.registerDisplayListener(displayListener, mainHandler);
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        String action = intent == null ? null : intent.getAction();
        if (ACTION_STOP.equals(action)) {
            stopCapture("Capture stopped");
            return START_NOT_STICKY;
        }
        if (!ACTION_START.equals(action)) {
            stopSelf(startId);
            return START_NOT_STICKY;
        }

        startForegroundForProjection();
        int resultCode = intent.getIntExtra(EXTRA_RESULT_CODE, 0);
        Intent resultData = readResultData(intent);
        if (resultData == null) {
            stopCapture("Missing screen-capture permission token");
            return START_NOT_STICKY;
        }
        String receiverHost = intent.getStringExtra(EXTRA_RECEIVER_HOST);
        String pairCode = intent.getStringExtra(EXTRA_PAIR_CODE);
        if (receiverHost == null || receiverHost.trim().isEmpty() ||
                !StreamProtocol.validPairCode(pairCode)) {
            stopCapture("Receiver address or pairing code is invalid");
            return START_NOT_STICKY;
        }
        startCapture(resultCode, resultData, receiverHost.trim(), pairCode);
        return START_NOT_STICKY;
    }

    private Intent readResultData(Intent intent) {
        if (Build.VERSION.SDK_INT >= 33) {
            return intent.getParcelableExtra(EXTRA_RESULT_DATA, Intent.class);
        }
        //noinspection deprecation
        return intent.getParcelableExtra(EXTRA_RESULT_DATA);
    }

    private void startForegroundForProjection() {
        Notification notification = buildNotification("Preparing H.264 encoder…");
        startForeground(NOTIFICATION_ID, notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);
    }

    private void startCapture(int resultCode, Intent resultData, String receiverHost,
                              String pairCode) {
        stopCaptureResources(true);
        stopping = false;
        encodedFrames.set(0);
        encodedBytes.set(0);
        lastStatusNanos = 0;

        try {
            MediaProjectionManager manager =
                    (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
            projection = manager.getMediaProjection(resultCode, resultData);
            if (projection == null) throw new IllegalStateException("MediaProjection was not granted");
            projection.registerCallback(projectionCallback, mainHandler);
            networkStreamer = new NetworkStreamer(receiverHost, StreamProtocol.DEFAULT_PORT,
                    pairCode, new NetworkStreamer.Listener() {
                @Override public void onConnectionChanged(boolean connected, String message) {
                    Log.i(TAG, message);
                    if (connected) RemoteControlAccessibilityService.resetSession();
                    mainHandler.post(() -> {
                        if (stopping) return;
                        EncoderSession current;
                        synchronized (encoderLock) { current = encoderSession; }
                        broadcastStatus(true, message, current == null ? null : current.size);
                    });
                }

                @Override public void onKeyFrameNeeded() {
                    requestSyncFrame();
                }

                @Override public void onRemoteInput(RemoteInputEvent event) {
                    if (!RemoteControlAccessibilityService.submit(event)) {
                        Log.w(TAG, "Remote input ignored: enable PhoneCast remote control and its accessibility service");
                    }
                }
            });
            networkStreamer.start();

            DisplayMetrics metrics = currentDisplayMetrics();
            densityDpi = metrics.densityDpi;
            CaptureConfig.Size size = CaptureConfig.fitWithinLimit(metrics.widthPixels, metrics.heightPixels);
            encoderSession = createEncoder(size);
            virtualDisplay = projection.createVirtualDisplay(
                    "PhoneCast screen capture", size.width, size.height, densityDpi,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                    encoderSession.surface, null, mainHandler);
            if (virtualDisplay == null) throw new IllegalStateException("Virtual display creation failed");

            setRunningPreference(true);
            String message = "Capturing and encoding H.264 at " + CaptureConfig.FRAME_RATE + " FPS";
            updateNotification(message);
            broadcastStatus(true, message, size);
            Log.i(TAG, message + " (" + size.width + "x" + size.height + ")");
        } catch (IOException | RuntimeException error) {
            Log.e(TAG, "Could not start capture", error);
            stopCapture("Capture failed: " + safeMessage(error));
        }
    }

    private EncoderSession createEncoder(CaptureConfig.Size size) throws IOException {
        MediaFormat format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC,
                size.width, size.height);
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        format.setInteger(MediaFormat.KEY_BIT_RATE, CaptureConfig.bitrateFor(size));
        format.setInteger(MediaFormat.KEY_FRAME_RATE, CaptureConfig.FRAME_RATE);
        // Surface producers may run at the display refresh rate despite KEY_FRAME_RATE.
        // This encoder-side cap drops excess input before H.264 dependencies are created.
        format.setFloat(MediaFormat.KEY_MAX_FPS_TO_ENCODER, CaptureConfig.FRAME_RATE);
        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,
                CaptureConfig.I_FRAME_INTERVAL_SECONDS);
        format.setInteger(MediaFormat.KEY_PRIORITY, 0);
        format.setInteger(MediaFormat.KEY_MAX_B_FRAMES, 0);
        if (Build.VERSION.SDK_INT >= 30) {
            format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1);
        }

        MediaCodec codec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC);
        MediaCodecInfo.CodecCapabilities capabilities = codec.getCodecInfo()
                .getCapabilitiesForType(MediaFormat.MIMETYPE_VIDEO_AVC);
        if (capabilities.getEncoderCapabilities().isBitrateModeSupported(
                MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR)) {
            format.setInteger(MediaFormat.KEY_BITRATE_MODE,
                    MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_CBR);
        }
        if (Build.VERSION.SDK_INT >= 30 && capabilities.isFeatureSupported(
                MediaCodecInfo.CodecCapabilities.FEATURE_LowLatency)) {
            format.setFeatureEnabled(MediaCodecInfo.CodecCapabilities.FEATURE_LowLatency, true);
        }
        EncoderSession session = new EncoderSession(codec, size);
        codec.setCallback(new MediaCodec.Callback() {
            @Override public void onInputBufferAvailable(MediaCodec mediaCodec, int index) {
                // Surface-input encoders do not expose input buffers.
            }

            @Override public void onOutputBufferAvailable(MediaCodec mediaCodec, int index,
                                                           MediaCodec.BufferInfo info) {
                handleEncodedOutput(session, mediaCodec, index, info);
            }

            @Override public void onError(MediaCodec mediaCodec, MediaCodec.CodecException error) {
                if (!session.active) return;
                Log.e(TAG, "H.264 encoder error", error);
                mainHandler.post(() -> stopCapture("Encoder failed: " + safeMessage(error)));
            }

            @Override public void onOutputFormatChanged(MediaCodec mediaCodec, MediaFormat format) {
                if (!session.active) return;
                Log.i(TAG, "H.264 output format: " + format);
                byte[] config = codecConfigFromFormat(format);
                NetworkStreamer streamer = networkStreamer;
                if (config.length > 0 && streamer != null && session.markConfigOffered()) {
                    streamer.offerConfig(config, session.size.width, session.size.height);
                    Log.i(TAG, "Sent H.264 configuration from output format (" +
                            session.size.width + "x" + session.size.height + ")");
                }
            }
        }, codecHandler);
        try {
            codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            session.surface = codec.createInputSurface();
            session.active = true;
            codec.start();
            return session;
        } catch (RuntimeException error) {
            session.release();
            throw error;
        }
    }

    private void handleEncodedOutput(EncoderSession session, MediaCodec codec, int index,
                                     MediaCodec.BufferInfo info) {
        if (!session.active) return;
        try {
            ByteBuffer output = codec.getOutputBuffer(index);
            if (output != null && info.size > 0) {
                output.position(info.offset);
                output.limit(info.offset + info.size);
                byte[] payload = new byte[info.size];
                output.get(payload);
                encodedBytes.addAndGet(info.size);
                NetworkStreamer streamer = networkStreamer;
                if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                    if (streamer != null && session.markConfigOffered()) {
                        streamer.offerConfig(payload, session.size.width, session.size.height);
                        Log.i(TAG, "Sent H.264 configuration from codec buffer (" +
                                session.size.width + "x" + session.size.height + ")");
                    }
                } else {
                    long sequence = encodedFrames.getAndIncrement();
                    int flags = (info.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0
                            ? StreamProtocol.FLAG_KEY_FRAME : 0;
                    if (streamer != null) {
                        streamer.offerFrame(payload, flags, sequence, info.presentationTimeUs,
                                session.size.width, session.size.height);
                    }
                }
            }
            codec.releaseOutputBuffer(index, false);
        } catch (IllegalStateException ignored) {
            return; // The encoder was replaced during a resize.
        }

        long now = System.nanoTime();
        if (now - lastStatusNanos >= 1_000_000_000L) {
            lastStatusNanos = now;
            synchronized (encoderLock) {
                if (encoderSession == session) {
                    broadcastStatus(true, "Capturing and encoding H.264", session.size);
                }
            }
        }
    }

    private void requestSyncFrame() {
        codecHandler.post(() -> {
            synchronized (encoderLock) {
                if (encoderSession == null) return;
                try {
                    android.os.Bundle parameters = new android.os.Bundle();
                    parameters.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
                    encoderSession.codec.setParameters(parameters);
                } catch (IllegalStateException error) {
                    Log.w(TAG, "Could not request an H.264 sync frame", error);
                }
            }
        });
    }

    private void reconfigureForSize(int sourceWidth, int sourceHeight) {
        if (stopping || projection == null || virtualDisplay == null ||
                sourceWidth <= 0 || sourceHeight <= 0) return;
        CaptureConfig.Size requested = CaptureConfig.fitWithinLimit(sourceWidth, sourceHeight);
        synchronized (encoderLock) {
            if (encoderSession != null && encoderSession.size.equals(requested)) return;
        }
        Log.i(TAG, "Reconfiguring capture for source " + sourceWidth + "x" + sourceHeight +
                " -> encoder " + requested.width + "x" + requested.height);

        try {
            virtualDisplay.setSurface(null);
            EncoderSession old;
            synchronized (encoderLock) {
                old = encoderSession;
                encoderSession = null;
            }
            if (old != null) old.release();

            EncoderSession replacement = createEncoder(requested);
            synchronized (encoderLock) {
                encoderSession = replacement;
            }
            virtualDisplay.resize(requested.width, requested.height, densityDpi);
            virtualDisplay.setSurface(replacement.surface);
            requestSyncFrame();
            broadcastStatus(true, "Capture resized after orientation/content change", requested);
            Log.i(TAG, "Capture resized to " + requested.width + "x" + requested.height);
        } catch (IOException | RuntimeException error) {
            Log.e(TAG, "Could not resize encoder", error);
            stopCapture("Could not adapt to screen rotation: " + safeMessage(error));
        }
    }

    @Override public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        scheduleCurrentDisplaySizeCheck();
    }

    private void scheduleCurrentDisplaySizeCheck() {
        if (stopping || projection == null) return;
        mainHandler.removeCallbacks(currentDisplaySizeCheck);
        mainHandler.post(currentDisplaySizeCheck);
        // Some vendors notify display/configuration listeners before WindowMetrics
        // reflects the new rotation. Check once more after that transition settles.
        mainHandler.postDelayed(currentDisplaySizeCheck, 250);
    }

    private final Runnable currentDisplaySizeCheck = () -> {
        if (stopping || projection == null) return;
        DisplayMetrics metrics = currentDisplayMetrics();
        reconfigureForSize(metrics.widthPixels, metrics.heightPixels);
    };

    private DisplayMetrics currentDisplayMetrics() {
        DisplayMetrics metrics = new DisplayMetrics();
        WindowManager windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        if (Build.VERSION.SDK_INT >= 30) {
            Rect bounds = windowManager.getCurrentWindowMetrics().getBounds();
            metrics.widthPixels = bounds.width();
            metrics.heightPixels = bounds.height();
            metrics.densityDpi = getResources().getConfiguration().densityDpi;
        } else {
            //noinspection deprecation
            windowManager.getDefaultDisplay().getRealMetrics(metrics);
        }
        return metrics;
    }

    private void stopCapture(String message) {
        if (stopping) return;
        stopping = true;
        stopCaptureResources(true);
        setRunningPreference(false);
        broadcastStatus(false, message, null);
        stopForeground(STOP_FOREGROUND_REMOVE);
        stopSelf();
        Log.i(TAG, message);
    }

    private void stopCaptureResources(boolean stopProjection) {
        if (virtualDisplay != null) {
            virtualDisplay.release();
            virtualDisplay = null;
        }
        EncoderSession old;
        synchronized (encoderLock) {
            old = encoderSession;
            encoderSession = null;
        }
        if (old != null) old.release();
        if (networkStreamer != null) {
            networkStreamer.stop();
            networkStreamer = null;
        }
        if (projection != null) {
            projection.unregisterCallback(projectionCallback);
            MediaProjection oldProjection = projection;
            projection = null;
            if (stopProjection) oldProjection.stop();
        }
    }

    @Override public void onDestroy() {
        stopping = true;
        mainHandler.removeCallbacks(currentDisplaySizeCheck);
        DisplayManager displayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
        displayManager.unregisterDisplayListener(displayListener);
        stopCaptureResources(true);
        setRunningPreference(false);
        if (codecThread != null) codecThread.quitSafely();
        super.onDestroy();
    }

    private void broadcastStatus(boolean running, String message, CaptureConfig.Size size) {
        Intent status = new Intent(ACTION_STATUS)
                .setPackage(getPackageName())
                .putExtra(EXTRA_RUNNING, running)
                .putExtra(EXTRA_MESSAGE, message)
                .putExtra(EXTRA_FRAMES, encodedFrames.get())
                .putExtra(EXTRA_BYTES, encodedBytes.get())
                .putExtra(EXTRA_DROPPED_FRAMES,
                        networkStreamer == null ? 0L : networkStreamer.droppedFrames())
                .putExtra(EXTRA_NETWORK_RTT_MICROS,
                        networkStreamer == null ? -1L : networkStreamer.roundTripMicros());
        if (size != null) {
            status.putExtra(EXTRA_WIDTH, size.width);
            status.putExtra(EXTRA_HEIGHT, size.height);
        }
        sendBroadcast(status);
    }

    private void createNotificationChannel() {
        NotificationChannel channel = new NotificationChannel(CHANNEL_ID,
                getString(R.string.capture_channel_name), NotificationManager.IMPORTANCE_LOW);
        channel.setDescription(getString(R.string.capture_channel_description));
        getSystemService(NotificationManager.class).createNotificationChannel(channel);
    }

    private Notification buildNotification(String text) {
        Intent openIntent = new Intent(this, MainActivity.class);
        PendingIntent open = PendingIntent.getActivity(this, 0, openIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Intent stopIntent = new Intent(this, ScreenCaptureService.class).setAction(ACTION_STOP);
        PendingIntent stop = PendingIntent.getService(this, 1, stopIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_phonecast)
                .setContentTitle(getString(R.string.capture_notification_title))
                .setContentText(text)
                .setContentIntent(open)
                .setOngoing(true)
                .setCategory(Notification.CATEGORY_SERVICE)
                .addAction(new Notification.Action.Builder(null,
                        getString(R.string.stop_casting), stop).build())
                .build();
    }

    private void updateNotification(String text) {
        getSystemService(NotificationManager.class).notify(NOTIFICATION_ID,
                buildNotification(text));
    }

    private void setRunningPreference(boolean running) {
        getSharedPreferences(PREFERENCES, MODE_PRIVATE).edit()
                .putBoolean("capture_running", running).apply();
    }

    private static byte[] codecConfigFromFormat(MediaFormat format) {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        for (int index = 0; index < 3; ++index) {
            ByteBuffer data = format.getByteBuffer("csd-" + index);
            if (data == null) continue;
            ByteBuffer copy = data.duplicate();
            byte[] part = new byte[copy.remaining()];
            copy.get(part);
            bytes.write(part, 0, part.length);
        }
        return bytes.toByteArray();
    }

    private static String safeMessage(Throwable error) {
        return error.getMessage() == null ? error.getClass().getSimpleName() : error.getMessage();
    }

    @Override public IBinder onBind(Intent intent) {
        return null;
    }

    private static final class EncoderSession {
        final MediaCodec codec;
        final CaptureConfig.Size size;
        volatile boolean active;
        private boolean configOffered;
        Surface surface;

        EncoderSession(MediaCodec codec, CaptureConfig.Size size) {
            this.codec = codec;
            this.size = size;
        }

        synchronized boolean markConfigOffered() {
            if (configOffered) return false;
            configOffered = true;
            return true;
        }

        void release() {
            active = false;
            try {
                codec.stop();
            } catch (IllegalStateException ignored) {
                // A codec may fail before it reaches the started state.
            }
            codec.release();
            if (surface != null) {
                surface.release();
                surface = null;
            }
        }
    }
}
