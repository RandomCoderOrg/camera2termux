# Camera2Termux

Camera2Termux is an experimental Android camera companion for native Termux programs. It runs [fauxdev](https://github.com/RandomCoderOrg/fauxdev)'s Camera2 broker in a camera foreground service and supplies a dummy `/dev/video0` V4L2 stream to programs launched with `camera2termux-run`.

The camera starts only when you tap **Start camera** in the app. The notification's **Stop** action closes it. The current stream is 320×240 RGB24; it is a compatibility prototype, not a general V4L2 implementation.

## Compatibility

The published build uses Termux's public GitHub development signing key and `com.termux` shared UID, so it installs alongside **GitHub-signed Termux only**. This local branch removes the shared UID and uses an authenticated FD handoff. A Pixel 6a (Android 17) with F-Droid Termux captured live frames, but this branch is not yet a release. The cross-UID handoff requires Android 14+; Android 12–13 remain unsupported until an authenticated alternative is implemented. Do not uninstall Termux or its data to try either build.

The public key does not establish trust in an APK: verify the source and artifact before installing. The expected APK signing certificate SHA-256 is `b6da01480eefd5fbf2cd3771b8d1021ec791304bdd6c4bf41d3faabad48ee5e1`.

## Build and use

Every APK and native library in CI is compiled from the source pinned in this repository. The [build workflow](.github/workflows/build.yml) fetches Termux's public key at a pinned commit, verifies its SHA-256, runs fauxdev's tests, builds the APK and Termux-native library, verifies the APK certificate, and uploads both artifacts. No locally compiled library is checked in or repackaged.

For the published build, install the CI APK on a device with GitHub-signed Termux. This local branch can be built and tested with F-Droid Termux, but has no published artifact. In Termux, extract the matching `camera2termux-termux-arm64.tar.gz`, then install its two files:

```sh
tar -xzf camera2termux-termux-arm64.tar.gz
install -m 755 libfauxdev-termux.so "$PREFIX/lib/libfauxdev-termux.so"
install -m 755 camera2termux-run "$PREFIX/bin/camera2termux-run"
```

Open Camera2Termux, grant camera permission, and tap **Start camera**. Then use `camera2termux-run COMMAND ...` for a native Termux program that opens `/dev/video0` through V4L2. The published launcher uses the app-private socket path; this branch instead hands an already-connected FD to the command and preloads the native shim. It does not enable the camera in arbitrary processes. Stop the service when finished.

PRoot/glibc programs need their own fauxdev build and setup; this ARM64 Bionic library is for native Termux programs. The local F-Droid/Pixel result does not establish compatibility across devices or Termux variants.
