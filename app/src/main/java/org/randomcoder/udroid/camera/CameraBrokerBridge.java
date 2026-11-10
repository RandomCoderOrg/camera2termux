package org.randomcoder.udroid.camera;

final class CameraBrokerBridge {
    static { System.loadLibrary("fauxdev-camera"); }
    static native boolean prepare();
    static native int run(String socketPath);
    static native void stop();
    private CameraBrokerBridge() {}
}
