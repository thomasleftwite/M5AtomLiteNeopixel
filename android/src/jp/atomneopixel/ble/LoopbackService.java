package jp.atomneopixel.ble;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.BindException;

public final class LoopbackService extends Service {
    enum Status { STOPPED, STARTING, RUNNING, PORT_BUSY, FAILED }

    private static final String CHANNEL = "atom_loopback";
    private static final String STOP = "jp.atomneopixel.ble.STOP";
    private static volatile Status status = Status.STOPPED;
    private final Handler main = new Handler(Looper.getMainLooper());
    private Thread worker;
    private LoopbackServer server;
    private boolean destroyed;

    static Status status() {
        return status;
    }

    static void start(Context context) {
        if (status == Status.STARTING || status == Status.RUNNING) return;
        status = Status.STARTING;
        try {
            context.startForegroundService(new Intent(context, LoopbackService.class));
        } catch (RuntimeException error) {
            status = Status.FAILED;
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && STOP.equals(intent.getAction())) {
            stopSelf();
            return START_NOT_STICKY;
        }
        startForeground(1, notification());
        if (worker == null) {
            status = Status.STARTING;
            worker = new Thread(this::runServer, "atom-loopback");
            worker.start();
        }
        return START_NOT_STICKY;
    }

    private Notification notification() {
        NotificationChannel channel = new NotificationChannel(
                CHANNEL, "BLE 操作画面", NotificationManager.IMPORTANCE_LOW);
        getSystemService(NotificationManager.class).createNotificationChannel(channel);
        int flags = PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT;
        PendingIntent openApp = PendingIntent.getActivity(
                this, 0, new Intent(this, LauncherActivity.class), flags);
        PendingIntent stop = PendingIntent.getService(
                this, 1, new Intent(this, LoopbackService.class).setAction(STOP), flags);
        return new Notification.Builder(this, CHANNEL)
                .setSmallIcon(R.drawable.atom)
                .setContentTitle("Atom NeoPixel")
                .setContentText("Chrome に BLE 操作画面を配信しています")
                .setContentIntent(openApp)
                .setOngoing(true)
                .addAction(new Notification.Action.Builder(null, "停止", stop).build())
                .build();
    }

    private byte[] readPage() throws IOException {
        try (InputStream input = getAssets().open("index.html")) {
            ByteArrayOutputStream page = new ByteArrayOutputStream();
            byte[] buffer = new byte[4096];
            for (int count; (count = input.read(buffer)) != -1; ) page.write(buffer, 0, count);
            return page.toByteArray();
        }
    }

    private void runServer() {
        try (LoopbackServer opened = new LoopbackServer(readPage())) {
            synchronized (this) {
                if (destroyed) return;
                server = opened;
                status = Status.RUNNING;
            }
            opened.serve();
        } catch (IOException error) {
            synchronized (this) {
                if (destroyed) return;
                status = error instanceof BindException ? Status.PORT_BUSY : Status.FAILED;
                main.post(this::stopSelf);
            }
        }
    }

    @Override
    public synchronized void onDestroy() {
        destroyed = true;
        if (server != null) {
            try {
                server.close();
            } catch (IOException ignored) {
                // The worker also releases the listener when it exits.
            }
        }
        if (status == Status.RUNNING || status == Status.STARTING) status = Status.STOPPED;
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
