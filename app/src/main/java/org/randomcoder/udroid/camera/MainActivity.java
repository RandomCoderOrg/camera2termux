package org.randomcoder.udroid.camera;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private static final int CAMERA_REQUEST = 1;
    private TextView status;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (24 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);
        TextView help = new TextView(this);
        help.setText("Camera2Termux\n\nStart the camera here, then run a native Termux program with camera2termux-run.\n\nSocket: "
                + new java.io.File(getFilesDir(), "camera.sock").getAbsolutePath());
        layout.addView(help);
        status = new TextView(this);
        status.setText("Camera is off");
        layout.addView(status);
        Button start = new Button(this);
        start.setText("Start camera");
        start.setOnClickListener(v -> {
            if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED)
                startCamera();
            else requestPermissions(new String[]{Manifest.permission.CAMERA}, CAMERA_REQUEST);
        });
        layout.addView(start);
        Button stop = new Button(this);
        stop.setText("Stop camera");
        stop.setOnClickListener(v -> {
            startService(new Intent(this, CameraService.class).setAction(CameraService.STOP));
            status.setText("Camera stopped");
        });
        layout.addView(stop);
        setContentView(layout);
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == CAMERA_REQUEST) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) startCamera();
            else status.setText("Camera permission denied");
        }
    }

    private void startCamera() {
        try {
            startForegroundService(new Intent(this, CameraService.class).setAction(CameraService.START));
            status.setText("Camera starting. Keep the notification visible while using it from Termux.");
        } catch (RuntimeException error) {
            status.setText("Camera could not start: " + error.getMessage());
        }
    }
}
