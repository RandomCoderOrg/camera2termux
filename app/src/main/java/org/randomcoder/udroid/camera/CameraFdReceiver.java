package org.randomcoder.udroid.camera;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.IBinder;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.util.Log;
import java.io.File;

/** Hands the running broker's socket to a Termux-side Binder callback. */
public final class CameraFdReceiver extends BroadcastReceiver {
    static final String ACTION = "org.randomcoder.camera2termux.REQUEST_FD";
    static final int TRANSACTION_FD = IBinder.FIRST_CALL_TRANSACTION;

    @Override public void onReceive(Context context, Intent intent) {
        // shortcut: sender UID is available only on API 34+, keep older Android closed until authenticated transport exists.
        if (!ACTION.equals(intent.getAction()) || Build.VERSION.SDK_INT < 34) return;
        int uid = getSentFromUid();
        Log.i("Camera2Termux", "Camera FD request from UID " + uid);
        int termuxUid;
        try {
            termuxUid = context.getPackageManager().getPackageUid("com.termux", 0);
        } catch (PackageManager.NameNotFoundException error) {
            return;
        }
        boolean debugShell = uid == Process.SHELL_UID &&
                (context.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
        if (uid != termuxUid && !debugShell) {
            Log.w("Camera2Termux", "Rejected camera FD request from UID " + uid);
            return;
        }
        IBinder callback = intent.getExtras() == null ? null : intent.getExtras().getBinder("callback");
        if (callback == null) return;
        File path = new File(context.getFilesDir(), "camera.sock");
        int socketFd = CameraBrokerBridge.connect(path.getAbsolutePath());
        if (socketFd < 0) {
            Log.e("Camera2Termux", "Camera broker connect failed: errno " + -socketFd);
            return;
        }
        try (ParcelFileDescriptor fd = ParcelFileDescriptor.adoptFd(socketFd)) {
            Parcel data = Parcel.obtain();
            try {
                fd.writeToParcel(data, 0);
                if (!callback.transact(TRANSACTION_FD, data, null, IBinder.FLAG_ONEWAY))
                    Log.e("Camera2Termux", "Camera FD callback rejected the transaction");
            } finally {
                data.recycle();
            }
        } catch (Exception error) {
            Log.e("Camera2Termux", "Camera FD handoff failed", error);
        }
    }

}
