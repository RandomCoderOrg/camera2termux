package org.randomcoder.udroid.camera;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.IBinder;
import android.util.Log;
import java.io.File;

public final class CameraService extends Service {
    static final String START = "org.randomcoder.camera2termux.START";
    static final String STOP = "org.randomcoder.camera2termux.STOP";
    private static final String CHANNEL = "camera";
    private Thread broker;

    @Override public void onCreate() {
        super.onCreate();
        getSystemService(NotificationManager.class).createNotificationChannel(
                new NotificationChannel(CHANNEL, "Camera2Termux", NotificationManager.IMPORTANCE_LOW));
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || STOP.equals(intent.getAction())) {
            stopSelf();
            return START_NOT_STICKY;
        }
        if (!START.equals(intent.getAction()) ||
                checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            stopSelf();
            return START_NOT_STICKY;
        }
        if (broker != null) return START_NOT_STICKY;
        Intent stop = new Intent(this, CameraService.class).setAction(STOP);
        PendingIntent stopAction = PendingIntent.getService(this, 0, stop,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification notification = new Notification.Builder(this, CHANNEL)
                .setSmallIcon(android.R.drawable.ic_menu_camera)
                .setContentTitle("Camera available to Termux")
                .setContentText("Camera2Termux is streaming only while this service runs")
                .setOngoing(true)
                .addAction(new Notification.Action.Builder(null, "Stop", stopAction).build())
                .build();
        try {
            startForeground(1, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA);
            if (!CameraBrokerBridge.prepare()) throw new IllegalStateException("broker is already active");
            File socket = new File(getFilesDir(), "camera.sock");
            broker = new Thread(() -> {
                int result = CameraBrokerBridge.run(socket.getAbsolutePath());
                Log.i("Camera2Termux", "Broker exited: " + result);
                stopSelf();
            }, "camera2termux-broker");
            broker.start();
        } catch (RuntimeException error) {
            Log.e("Camera2Termux", "Cannot start camera broker", error);
            CameraBrokerBridge.stop();
            stopSelf();
        }
        return START_NOT_STICKY;
    }

    @Override public void onDestroy() {
        CameraBrokerBridge.stop();
        broker = null;
        super.onDestroy();
    }

    @Override public IBinder onBind(Intent intent) { return null; }
}
