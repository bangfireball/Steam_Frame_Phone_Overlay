package com.phonecastvr.sender;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.Locale;

public final class MainActivity extends Activity {
    private static final int REQUEST_CAPTURE = 1001;
    private static final int REQUEST_NOTIFICATIONS = 1002;

    private TextView statusView;
    private TextView metricsView;
    private Button startButton;
    private Button stopButton;
    private boolean captureAfterPermission;

    private final BroadcastReceiver statusReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context context, Intent intent) {
            boolean running = intent.getBooleanExtra(ScreenCaptureService.EXTRA_RUNNING, false);
            String message = intent.getStringExtra(ScreenCaptureService.EXTRA_MESSAGE);
            long frames = intent.getLongExtra(ScreenCaptureService.EXTRA_FRAMES, 0L);
            long bytes = intent.getLongExtra(ScreenCaptureService.EXTRA_BYTES, 0L);
            int width = intent.getIntExtra(ScreenCaptureService.EXTRA_WIDTH, 0);
            int height = intent.getIntExtra(ScreenCaptureService.EXTRA_HEIGHT, 0);
            updateState(running, message, frames, bytes, width, height);
        }
    };

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
        boolean running = getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE)
                .getBoolean("capture_running", false);
        updateState(running, running ? "Capture service active" : "Ready to capture", 0, 0, 0, 0);
    }

    @Override protected void onStart() {
        super.onStart();
        registerStatusReceiver();
    }

    @SuppressLint("UnspecifiedRegisterReceiverFlag")
    private void registerStatusReceiver() {
        IntentFilter filter = new IntentFilter(ScreenCaptureService.ACTION_STATUS);
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(statusReceiver, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            // The flags overload was introduced in API 33; the broadcast is package-scoped.
            registerReceiver(statusReceiver, filter);
        }
    }

    @Override protected void onStop() {
        unregisterReceiver(statusReceiver);
        super.onStop();
    }

    private void buildUi() {
        int padding = dp(24);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(padding, padding * 2, padding, padding);
        root.setBackgroundColor(Color.rgb(10, 16, 28));

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextColor(Color.WHITE);
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        root.addView(title, matchWrap());

        TextView explanation = new TextView(this);
        explanation.setText(R.string.sprint_two_explanation);
        explanation.setTextColor(Color.rgb(190, 205, 220));
        explanation.setTextSize(16);
        explanation.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams explanationParams = matchWrap();
        explanationParams.setMargins(0, dp(16), 0, dp(24));
        root.addView(explanation, explanationParams);

        statusView = new TextView(this);
        statusView.setTextColor(Color.rgb(42, 188, 251));
        statusView.setTextSize(18);
        statusView.setGravity(Gravity.CENTER);
        root.addView(statusView, matchWrap());

        metricsView = new TextView(this);
        metricsView.setTextColor(Color.WHITE);
        metricsView.setTextSize(15);
        metricsView.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams metricsParams = matchWrap();
        metricsParams.setMargins(0, dp(12), 0, dp(24));
        root.addView(metricsView, metricsParams);

        startButton = new Button(this);
        startButton.setText(R.string.start_capture);
        startButton.setOnClickListener(view -> requestCapture());
        root.addView(startButton, matchWrap());

        stopButton = new Button(this);
        stopButton.setText(R.string.stop_capture);
        stopButton.setOnClickListener(view -> stopCapture());
        LinearLayout.LayoutParams stopParams = matchWrap();
        stopParams.setMargins(0, dp(12), 0, 0);
        root.addView(stopButton, stopParams);

        setContentView(root);
    }

    private void requestCapture() {
        if (Build.VERSION.SDK_INT >= 33 &&
                checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            captureAfterPermission = true;
            requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQUEST_NOTIFICATIONS);
            return;
        }
        launchCaptureConsent();
    }

    private void launchCaptureConsent() {
        MediaProjectionManager manager =
                (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
        startActivityForResult(manager.createScreenCaptureIntent(), REQUEST_CAPTURE);
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                                      int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_NOTIFICATIONS && captureAfterPermission) {
            captureAfterPermission = false;
            launchCaptureConsent();
        }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_CAPTURE) return;
        if (resultCode != RESULT_OK || data == null) {
            updateState(false, "Screen-capture permission was not granted", 0, 0, 0, 0);
            return;
        }
        Intent service = new Intent(this, ScreenCaptureService.class)
                .setAction(ScreenCaptureService.ACTION_START)
                .putExtra(ScreenCaptureService.EXTRA_RESULT_CODE, resultCode)
                .putExtra(ScreenCaptureService.EXTRA_RESULT_DATA, data);
        startForegroundService(service);
        updateState(true, "Starting capture…", 0, 0, 0, 0);
    }

    private void stopCapture() {
        Intent service = new Intent(this, ScreenCaptureService.class)
                .setAction(ScreenCaptureService.ACTION_STOP);
        startService(service);
    }

    private void updateState(boolean running, String message, long frames, long bytes,
                             int width, int height) {
        getSharedPreferences(ScreenCaptureService.PREFERENCES, MODE_PRIVATE).edit()
                .putBoolean("capture_running", running).apply();
        statusView.setText(message == null ? (running ? "Capturing" : "Stopped") : message);
        if (running && width > 0) {
            metricsView.setText(String.format(Locale.US,
                    "%d × %d · %,d encoded frames · %.1f MB", width, height, frames,
                    bytes / 1_000_000.0));
        } else {
            metricsView.setText(running ? "Initializing H.264 encoder…" : "No active capture");
        }
        startButton.setEnabled(!running);
        stopButton.setEnabled(running);
    }

    private LinearLayout.LayoutParams matchWrap() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
