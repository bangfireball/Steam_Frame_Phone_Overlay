package com.phonecastvr.sender;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import java.io.File;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/** Only app-generated safe events are recorded; no system logcat or private payloads. */
final class DiagnosticLog {
    private static final int LIMIT = 65536;
    private static long lastSample;
    static synchronized void record(Context context, String safeEvent) {
        try {
            File file = new File(context.getFilesDir(), "support.log");
            String previous = file.isFile() ? new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8) : "";
            String text = previous + System.currentTimeMillis() + " " + safeEvent + "\n";
            if (text.length() > LIMIT) {
                text = text.substring(text.length() - LIMIT);
                int newline = text.indexOf('\n');
                if (newline >= 0) text = text.substring(newline + 1);
            }
            Files.write(file.toPath(), text.getBytes(StandardCharsets.UTF_8));
        } catch (Exception ignored) { /* Logging must not interrupt casting. */ }
    }
    static synchronized void sample(Context context, boolean active, boolean responsive,
            long frames, long dropped, long rtt) {
        long now = android.os.SystemClock.elapsedRealtime();
        if (active && now - lastSample < 5000) return;
        lastSample = now;
        record(context, "capture=" + active + " receiver=" + responsive + " encoded=" + frames +
                " local_drops=" + dropped + " rtt_us=" + rtt);
    }
    static String version(Context context) {
        try {
            android.content.pm.PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            return info.versionName + " (code " + info.getLongVersionCode() + ")";
        } catch (Exception ignored) { return "unknown"; }
    }
    static synchronized String export(Context context) throws Exception {
        String name = "phonecast-android-" + new SimpleDateFormat("yyyyMMdd-HHmmss-SSS", Locale.US).format(new Date()) + ".txt";
        File file = new File(context.getFilesDir(), "support.log");
        String report = "PhoneCast " + version(context) + "\nAndroid API " + Build.VERSION.SDK_INT +
                "\nBuild " + BuildConfig.SUPPORT_BUILD + " " + BuildConfig.BUILD_TYPE + "\nSafe app diagnostics; no screen/audio samples, IPs or pairing codes.\n" +
                (file.isFile() ? new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8) : "No events recorded.\n");
        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
        values.put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
        values.put(MediaStore.MediaColumns.IS_PENDING, 1);
        Uri uri = context.getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        if (uri == null) throw new java.io.IOException("Unable to create download");
        try {
            try (OutputStream output = context.getContentResolver().openOutputStream(uri)) {
                if (output == null) throw new java.io.IOException("Unable to open download");
                output.write(report.getBytes(StandardCharsets.UTF_8));
            }
            values.clear(); values.put(MediaStore.MediaColumns.IS_PENDING, 0);
            if (context.getContentResolver().update(uri, values, null, null) != 1)
                throw new java.io.IOException("Unable to finish download");
            return "Downloads/" + name;
        } catch (Exception error) {
            context.getContentResolver().delete(uri, null, null);
            throw error;
        }
    }
    private DiagnosticLog() {}
}
