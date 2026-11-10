package org.randomcoder.udroid.camera;

import android.content.Context;
import android.content.ComponentName;
import android.content.Intent;
import android.app.BroadcastOptions;
import android.content.pm.ApplicationInfo;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.system.Os;
import android.system.OsConstants;
import java.io.File;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Receives an authorized broker FD, then replaces this app_process helper with a Termux command. */
public final class CameraTermuxLauncher {
    public static void main(String[] args) throws Exception {
        if (args.length == 0) throw new IllegalArgumentException("Usage: camera2termux-run COMMAND [ARG...]");
        Looper.prepareMainLooper();
        // app_process has no app Context; use Termux's UID and package identity for the broadcast.
        Class<?> threadClass = Class.forName("android.app.ActivityThread");
        Object thread = threadClass.getMethod("systemMain").invoke(null);
        Context system = (Context) threadClass.getMethod("getSystemContext").invoke(thread);
        String caller = Process.myUid() == Process.SHELL_UID ? "com.android.shell" : "com.termux";
        ApplicationInfo info = system.getPackageManager().getApplicationInfo(caller, 0);
        Object loaded = threadClass.getMethod("getPackageInfoNoCheck", ApplicationInfo.class).invoke(thread, info);
        Method create = Class.forName("android.app.ContextImpl").getDeclaredMethod(
                "createAppContext", threadClass, Class.forName("android.app.LoadedApk"));
        create.setAccessible(true);
        Context context = (Context) create.invoke(null, thread, loaded);
        CountDownLatch received = new CountDownLatch(1);
        AtomicReference<ParcelFileDescriptor> brokerFd = new AtomicReference<>();
        AtomicReference<Exception> failure = new AtomicReference<>();
        IBinder callback = new Binder() {
            @Override protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) {
                if (code != CameraFdReceiver.TRANSACTION_FD) return false;
                ParcelFileDescriptor fd = null;
                try {
                    fd = ParcelFileDescriptor.CREATOR.createFromParcel(data);
                    if (!OsConstants.S_ISSOCK(Os.fstat(fd.getFileDescriptor()).st_mode))
                        throw new IllegalStateException("Received descriptor is not a socket");
                    brokerFd.set(fd);
                    fd = null;
                } catch (Exception error) {
                    failure.set(error);
                } finally {
                    if (fd != null) try { fd.close(); } catch (Exception ignored) { }
                }
                received.countDown();
                return true;
            }
        };
        Bundle extras = new Bundle();
        extras.putBinder("callback", callback);
        Intent request = new Intent(CameraFdReceiver.ACTION)
                .setComponent(new ComponentName("org.randomcoder.camera2termux",
                        "org.randomcoder.udroid.camera.CameraFdReceiver"))
                .putExtras(extras);
        context.sendBroadcast(request, null, BroadcastOptions.makeBasic()
                .setShareIdentityEnabled(true).toBundle());
        if (!received.await(10, TimeUnit.SECONDS)) throw new IllegalStateException("No camera FD received");
        if (failure.get() != null) throw failure.get();
        try (ParcelFileDescriptor fd = brokerFd.get()) {
            if (fd == null) throw new IllegalStateException("Camera FD callback returned nothing");
            String prefix = System.getenv("PREFIX");
            if (prefix == null || prefix.isEmpty()) prefix = "/data/data/com.termux/files/usr";
            String library = prefix + "/lib/libfauxdev-termux.so";
            if (!new File(library).isFile())
                throw new IllegalStateException("Missing Termux camera library: " + library);
            String command = resolveCommand(args[0], System.getenv("PATH"));
            int flags = Os.fcntlInt(fd.getFileDescriptor(), OsConstants.F_GETFD, 0);
            Os.fcntlInt(fd.getFileDescriptor(), OsConstants.F_SETFD, flags & ~OsConstants.FD_CLOEXEC);
            ArrayList<String> environment = new ArrayList<>();
            for (Map.Entry<String, String> entry : System.getenv().entrySet()) {
                if (!entry.getKey().equals("LD_PRELOAD") && !entry.getKey().equals("FAUXDEV_VIDEO_FD") &&
                        !entry.getKey().equals("FAUXDEV_VIDEO_SOCKET"))
                    environment.add(entry.getKey() + "=" + entry.getValue());
            }
            String preload = System.getenv("LD_PRELOAD");
            environment.add("LD_PRELOAD=" + library + (preload == null ? "" : ":" + preload));
            environment.add("FAUXDEV_VIDEO_FD=" + fd.getFd());
            Os.execve(command, args, environment.toArray(new String[0]));
        }
    }

    private static String resolveCommand(String name, String path) {
        if (name.contains("/")) return new File(name).getAbsolutePath();
        if (path != null) for (String directory : path.split(":")) {
            File candidate = new File(directory, name);
            if (candidate.isFile() && candidate.canExecute()) return candidate.getAbsolutePath();
        }
        throw new IllegalArgumentException("Command not found: " + name);
    }
}
