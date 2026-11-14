# Camera2Termux

Camera2Termux is an experimental Android camera companion for native Termux programs. It runs [fauxdev](https://github.com/RandomCoderOrg/fauxdev)'s Camera2 broker in a camera foreground service and supplies a dummy `/dev/video0` V4L2 stream to programs launched with `camera2termux-run`.

The camera starts only when you tap **Start camera** in the app. The notification's **Stop** action closes it. This branch exposes selectable RGB24 capture sizes from the selected Camera2 input, with a converted 320×240 fallback and a 128 MiB MMAP-buffer budget. It remains a capture prototype, not a general V4L2 implementation.

## Compatibility

Earlier published builds use Termux's public GitHub development signing key and `com.termux` shared UID, so they install alongside **GitHub-signed Termux only**. Current source removes the shared UID and uses an authenticated FD handoff. A Pixel 6a (Android 17) with F-Droid Termux captured live frames, but this is not yet a release. The cross-UID handoff requires Android 14+; Android 12–13 remain unsupported until an authenticated alternative is implemented. Do not uninstall Termux or its data to try either build.

The public key does not establish trust in an APK: verify the source and artifact before installing. The expected APK signing certificate SHA-256 is `b6da01480eefd5fbf2cd3771b8d1021ec791304bdd6c4bf41d3faabad48ee5e1`.

## Build and use

Every APK and native library in CI is compiled from the source pinned in this repository. The [build workflow](.github/workflows/build.yml) fetches Termux's public key at a pinned commit, verifies its SHA-256, runs fauxdev's tests, builds the APK and Termux-native library, compile-checks Studio on Linux, verifies the APK certificate, and uploads only the APK, Termux archive, and signature file. No locally compiled library is checked in or repackaged.

For earlier published builds, use GitHub-signed Termux. Current source and its matching CI artifacts support the authenticated F-Droid Termux handoff on Android 14+. In Termux, extract `camera2termux-termux-arm64.tar.gz`, then install its two files:

```sh
tar -xzf camera2termux-termux-arm64.tar.gz
install -m 755 libfauxdev-termux.so "$PREFIX/lib/libfauxdev-termux.so"
install -m 755 camera2termux-run "$PREFIX/bin/camera2termux-run"
```

Open Camera2Termux, grant camera permission, and tap **Start camera**. Then use `camera2termux-run COMMAND ...` for a native Termux program that opens `/dev/video0` through V4L2. The current launcher hands an already-connected FD to the command and preloads the native shim; earlier releases used an app-private socket path. It does not enable the camera in arbitrary processes. Stop the service when finished.

PRoot/glibc programs need their own fauxdev build and setup; this ARM64 Bionic library is for native Termux programs. The local F-Droid/Pixel result does not establish compatibility across devices or Termux variants.

## Studio (Termux:X11)

The experimental Linux desktop Studio uses the same `/dev/video0` stream for its live preview, PNG photos, and silent H.264 MP4 recording. Camera, RGB24 capture size, and fixed target FPS choices come from V4L2 enumeration; the on-screen delivered FPS is measured separately. Photos and video use the full selected capture size; only the preview is scaled. Studio uses Camera2's reported sensor orientation for its portrait-default output and offers a manual quarter-turn correction that applies to preview, PNG, and MP4. Raw V4L2 frames remain in sensor orientation; Studio does not infer the physical phone rotation from Termux:X11. The Properties pane shows working V4L2 values and an available Camera2 report, not a complete Camera2 metadata browser.

Studio is currently **source-build-only in native Termux**. The APK/NDK CI artifact does not contain a locally compiled Studio binary. Install Termux:X11, Qt 6 Quick/Controls, CMake, a C++ compiler, and FFmpeg with `libx264`, then build from this checkout:

```sh
pkg install x11-repo
pkg install qt6-qtbase qt6-qtdeclarative cmake clang ffmpeg
bash scripts/build-studio-termux.sh
```

Start Camera2Termux's camera service and Termux:X11, set `DISPLAY` to the active X11 display, then run `camera2termux-run camera2termux-studio`. Choose an existing writable save folder in Studio. MP4 is first written as a `.part` file and moved into place only after FFmpeg exits successfully. Recording drops whole frames if the encoder cannot keep up; the readout shows the drop count. On the tested Pixel 6a, 640×480 recording encoded about 11 frames/s while the camera delivered about 30 frames/s; a smaller size recorded more smoothly. No audio is recorded. Stop the Android camera service when finished.
