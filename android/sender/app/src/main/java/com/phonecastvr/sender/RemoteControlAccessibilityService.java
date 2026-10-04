package com.phonecastvr.sender;

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.graphics.Path;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityEvent;

public final class RemoteControlAccessibilityService extends AccessibilityService {
    static final String PREFERENCE_REMOTE_CONTROL = "remote_control_enabled";
    private static final String TAG = "PhoneCastRemote";
    private static volatile RemoteControlAccessibilityService instance;

    private float downX;
    private float downY;
    private float lastX;
    private float lastY;
    private long downTime;
    private boolean pointerDown;
    private boolean finishPointerGesture;
    private boolean gestureDispatchInFlight;
    private float dispatchedX;
    private float dispatchedY;
    private GestureDescription.StrokeDescription activeStroke;
    private int pointerGestureGeneration;
    private long lastSequence = -1;

    static boolean isConnected() {
        return instance != null;
    }

    static void resetSession() {
        RemoteControlAccessibilityService service = instance;
        if (service != null) service.getMainExecutor().execute(() -> {
            service.lastSequence = -1;
            service.resetPointerGesture();
        });
    }

    static boolean submit(RemoteInputEvent event) {
        RemoteControlAccessibilityService service = instance;
        if (service == null || !service.getSharedPreferences(
                ScreenCaptureService.PREFERENCES, MODE_PRIVATE)
                .getBoolean(PREFERENCE_REMOTE_CONTROL, false)) return false;
        service.getMainExecutor().execute(() -> service.handle(event));
        return true;
    }

    @Override protected void onServiceConnected() {
        instance = this;
        ScreenCaptureService.remoteControlStatusChanged(this);
        Log.i(TAG, "Remote-control accessibility service connected");
    }

    @Override public void onDestroy() {
        if (instance == this) instance = null;
        ScreenCaptureService.remoteControlStatusChanged(this);
        super.onDestroy();
    }

    @Override public void onAccessibilityEvent(AccessibilityEvent event) {
        // PhoneCast injects only user-requested gestures and does not inspect UI content.
    }

    @Override public void onInterrupt() {
        resetPointerGesture();
    }

    private void handle(RemoteInputEvent event) {
        if (event.sequence <= lastSequence) return;
        lastSequence = event.sequence;
        switch (event.type) {
            case RemoteInputEvent.DOWN:
                resetPointerGesture();
                downX = lastX = event.normalizedX;
                downY = lastY = event.normalizedY;
                dispatchedX = downX;
                dispatchedY = downY;
                downTime = SystemClock.uptimeMillis();
                pointerDown = true;
                break;
            case RemoteInputEvent.MOVE:
                if (pointerDown) {
                    lastX = event.normalizedX;
                    lastY = event.normalizedY;
                    dispatchNextPointerSegment();
                }
                break;
            case RemoteInputEvent.UP:
                if (pointerDown) {
                    lastX = event.normalizedX;
                    lastY = event.normalizedY;
                    pointerDown = false;
                    if (activeStroke == null && !gestureDispatchInFlight) {
                        long duration = Math.max(50L, Math.min(1500L,
                                SystemClock.uptimeMillis() - downTime));
                        dispatchNormalizedGesture(downX, downY, lastX, lastY, duration);
                    } else {
                        finishPointerGesture = true;
                        dispatchNextPointerSegment();
                    }
                }
                break;
            case RemoteInputEvent.SCROLL:
                dispatchScroll(event.normalizedX, event.normalizedY, event.scrollDelta);
                break;
            case RemoteInputEvent.BACK:
                performGlobalAction(GLOBAL_ACTION_BACK);
                resetPointerGesture();
                break;
            default:
                break;
        }
    }

    private void dispatchNextPointerSegment() {
        if (gestureDispatchInFlight) return;
        if (activeStroke == null) {
            if (!pointerDown || distance(dispatchedX, dispatchedY, lastX, lastY) < 0.003f) return;
            Path path = normalizedPath(dispatchedX, dispatchedY, lastX, lastY);
            activeStroke = new GestureDescription.StrokeDescription(path, 0, 32L, true);
            dispatchedX = lastX;
            dispatchedY = lastY;
            dispatchPointerStroke(false);
            return;
        }

        if (pointerDown && distance(dispatchedX, dispatchedY, lastX, lastY) < 0.002f) return;
        boolean completes = !pointerDown && finishPointerGesture;
        Path path = normalizedPath(dispatchedX, dispatchedY, lastX, lastY);
        activeStroke = activeStroke.continueStroke(path, 0, completes ? 16L : 32L, !completes);
        dispatchedX = lastX;
        dispatchedY = lastY;
        dispatchPointerStroke(completes);
    }

    private void dispatchPointerStroke(boolean completes) {
        gestureDispatchInFlight = true;
        final int generation = pointerGestureGeneration;
        GestureDescription gesture = new GestureDescription.Builder()
                .addStroke(activeStroke)
                .build();
        boolean accepted = dispatchGesture(gesture, new GestureResultCallback() {
            @Override public void onCompleted(GestureDescription description) {
                if (generation != pointerGestureGeneration) return;
                gestureDispatchInFlight = false;
                if (completes) {
                    activeStroke = null;
                    finishPointerGesture = false;
                } else {
                    dispatchNextPointerSegment();
                }
            }

            @Override public void onCancelled(GestureDescription description) {
                if (generation != pointerGestureGeneration) return;
                Log.w(TAG, "Android cancelled a continued remote gesture");
                resetPointerGesture();
            }
        }, null);
        if (!accepted) {
            Log.w(TAG, "Android rejected a continued remote gesture");
            resetPointerGesture();
        }
    }

    private Path normalizedPath(float startX, float startY, float endX, float endY) {
        DisplayMetrics metrics = displayMetrics();
        float width = Math.max(1, metrics.widthPixels - 1);
        float height = Math.max(1, metrics.heightPixels - 1);
        Path path = new Path();
        path.moveTo(clamp(startX) * width, clamp(startY) * height);
        path.lineTo(clamp(endX) * width, clamp(endY) * height);
        return path;
    }

    private void resetPointerGesture() {
        ++pointerGestureGeneration;
        pointerDown = false;
        finishPointerGesture = false;
        gestureDispatchInFlight = false;
        activeStroke = null;
    }

    private static float distance(float x1, float y1, float x2, float y2) {
        return (float) Math.hypot(x2 - x1, y2 - y1);
    }

    private void dispatchScroll(float x, float y, float delta) {
        if (Math.abs(delta) < 0.01f) return;
        float travel = Math.max(0.18f, Math.min(0.55f, Math.abs(delta) * 0.55f));
        float endY = clamp(y - Math.copySign(travel, delta));
        dispatchNormalizedGesture(x, y, x, endY, 220L);
    }

    private void dispatchNormalizedGesture(float startX, float startY,
                                           float endX, float endY, long durationMillis) {
        DisplayMetrics metrics = displayMetrics();
        Path path = new Path();
        path.moveTo(clamp(startX) * Math.max(1, metrics.widthPixels - 1),
                clamp(startY) * Math.max(1, metrics.heightPixels - 1));
        path.lineTo(clamp(endX) * Math.max(1, metrics.widthPixels - 1),
                clamp(endY) * Math.max(1, metrics.heightPixels - 1));
        GestureDescription gesture = new GestureDescription.Builder()
                .addStroke(new GestureDescription.StrokeDescription(path, 0, durationMillis))
                .build();
        if (!dispatchGesture(gesture, null, null)) {
            Log.w(TAG, "Android rejected a remote gesture");
        }
    }

    private DisplayMetrics displayMetrics() {
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

    private static float clamp(float value) {
        return Math.max(0.0f, Math.min(1.0f, value));
    }
}
