package com.siggmaez.aibuilder.bridge;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.os.*;
import com.chaquo.python.*;
import com.chaquo.python.android.AndroidPlatform;
import java.io.*;
import java.util.concurrent.*;
import javax.net.ssl.*;
import java.security.KeyStore;
import java.nio.charset.StandardCharsets;
import android.util.Base64;

public final class BridgeService extends Service {
    public static final String START = "bridge.start", STOP = "bridge.stop", SIGN_IN = "bridge.signin";
    public static volatile String lastError = "";
    public static volatile String snapshot = "{\"running\":false,\"account\":{}}";
    private final ScheduledExecutorService worker = Executors.newSingleThreadScheduledExecutor();
    private PyObject runtime;
    private boolean started;

    @Override public void onCreate() {
        super.onCreate();
        var manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("bridge", "Local AI bridge", NotificationManager.IMPORTANCE_LOW));
        var open = PendingIntent.getActivity(this, 0, new Intent(this, MainActivity.class), PendingIntent.FLAG_IMMUTABLE);
        var stop = PendingIntent.getService(this, 1, new Intent(this, BridgeService.class).setAction(STOP), PendingIntent.FLAG_IMMUTABLE);
        var notification = new Notification.Builder(this, "bridge").setContentTitle("AI Builder Bridge")
            .setContentText("Local bridge on this phone. Tap to open controls.")
            .setSmallIcon(android.R.drawable.ic_dialog_info).setContentIntent(open).setOngoing(true)
            .addAction(new Notification.Action.Builder(null, "Stop", stop).build()).build();
        if (Build.VERSION.SDK_INT >= 34) startForeground(8765, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        else startForeground(8765, notification);
        worker.scheduleWithFixedDelay(() -> {
            try { if (runtime != null) snapshot = runtime.callAttr("status").toString(); }
            catch (Exception ignored) { lastError = "Unable to read bridge status."; }
        }, 0, 1, TimeUnit.SECONDS);
    }

    @Override public int onStartCommand(Intent intent, int flags, int id) {
        String action = intent == null ? START : intent.getAction();
        worker.execute(() -> {
            try {
                if (STOP.equals(action)) { shutdown(); return; }
                if (!started) {
                    lastError = "";
                    File destination = new File(getFilesDir(), "bridge");
                    copyAssets("bridge", destination);
                    writeSystemCertificates();
                    if (!Python.isStarted()) Python.start(new AndroidPlatform(this));
                    runtime = Python.getInstance().getModule("phone_runtime");
                    snapshot = runtime.callAttr("start", getApplicationInfo().nativeLibraryDir,
                        getFilesDir().getAbsolutePath(), getCacheDir().getAbsolutePath()).toString();
                    started = true;
                }
                if (SIGN_IN.equals(action)) runtime.callAttr("sign_in");
            } catch (Exception error) {
                lastError = "Bridge could not start. " + error.getMessage();
                started = false;
                if (runtime != null) snapshot = runtime.callAttr("status").toString();
                stopForeground(STOP_FOREGROUND_REMOVE); stopSelf();
            }
        });
        return START_NOT_STICKY;
    }

    private void copyAssets(String source, File destination) throws IOException {
        String[] children = getAssets().list(source);
        if (children != null && children.length > 0) {
            if (!destination.isDirectory() && !destination.mkdirs()) throw new IOException("Cannot create private bridge directory.");
            for (String child : children) copyAssets(source + "/" + child, new File(destination, child));
        } else {
            try (InputStream input = getAssets().open(source); OutputStream output = new FileOutputStream(destination)) {
                byte[] buffer = new byte[16384]; int length;
                while ((length = input.read(buffer)) != -1) output.write(buffer, 0, length);
            }
        }
    }
    private void writeSystemCertificates() throws Exception {
        var factory=TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
        factory.init((KeyStore)null);
        try (var output=new FileOutputStream(new File(getFilesDir(),"system-ca.pem"))) {
            for (TrustManager manager:factory.getTrustManagers()) if(manager instanceof X509TrustManager trust) {
                for (var certificate:trust.getAcceptedIssuers()) {
                    String pem="-----BEGIN CERTIFICATE-----\n"+Base64.encodeToString(certificate.getEncoded(),Base64.NO_WRAP)+"\n-----END CERTIFICATE-----\n";
                    output.write(pem.getBytes(StandardCharsets.US_ASCII));
                }
            }
        }
    }
    private void shutdown() {
        if (runtime != null) runtime.callAttr("stop");
        started = false; snapshot = "{\"running\":false,\"account\":{}}";
        stopForeground(STOP_FOREGROUND_REMOVE); stopSelf();
    }
    @Override public void onDestroy() {
        worker.execute(() -> { if (runtime != null) runtime.callAttr("stop"); });
        worker.shutdown(); super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
