package org.randomcoder.udroid.camera;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Typeface;
import android.os.Bundle;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public final class MainActivity extends Activity {
    private static final int CAMERA_REQUEST = 1;
    private static final int INVENTORY_REQUEST = 2;
    private TextView status;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (24 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);
        TextView help = new TextView(this);
        help.setText("Camera2Termux\n\nStart the camera here, then use camera2termux-run in native Termux. Camera sharing is experimental; stop the service when finished.");
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
        Button inventory = new Button(this);
        inventory.setText("Camera inventory");
        inventory.setOnClickListener(v -> {
            if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED)
                showInventory();
            else requestPermissions(new String[]{Manifest.permission.CAMERA}, INVENTORY_REQUEST);
        });
        layout.addView(inventory);
        setContentView(layout);
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == CAMERA_REQUEST) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) startCamera();
            else status.setText("Camera permission denied");
        } else if (requestCode == INVENTORY_REQUEST) {
            if (results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) showInventory();
            else status.setText("Camera permission denied");
        }
    }

    private void showInventory() {
        String report = CameraBrokerBridge.inventory();
        TextView text = new TextView(this);
        text.setText(report);
        text.setTextIsSelectable(true);
        text.setTypeface(Typeface.MONOSPACE);
        int padding = (int) (16 * getResources().getDisplayMetrics().density);
        text.setPadding(padding, padding, padding, padding);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(text);
        new AlertDialog.Builder(this)
                .setTitle("Camera inventory")
                .setView(scroll)
                .setPositiveButton("Close", null)
                .show();
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
