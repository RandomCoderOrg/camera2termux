# Camera2Termux

Camera2Termux is an experimental Android camera companion for native Termux programs. It runs [fauxdev](https://github.com/RandomCoderOrg/fauxdev)'s Camera2 broker in a camera foreground service and supplies a dummy `/dev/video0` V4L2 stream to programs launched with `camera2termux-run`.

The camera starts only when you tap **Start camera** in the app. The notification's **Stop** action closes it. This branch exposes selectable RGB24 and NV12 capture sizes from the selected Camera2 input, with a scaled 320×240 fallback and a 128 MiB MMAP-buffer budget. It remains a capture prototype, not a general V4L2 implementation.

## Compatibility

Earlier published builds use Termux's public GitHub development signing key and `com.termux` shared UID, so they install alongside **GitHub-signed Termux only**. Current source removes the shared UID and uses an authenticated FD handoff. A Pixel 6a (Android 17) with F-Droid Termux captured live frames, but this is not yet a release. The cross-UID handoff requires Android 14+; Android 12–13 remain unsupported until an authenticated alternative is implemented. Do not uninstall Termux or its data to try either build.

The public key does not establish trust in an APK: verify the source and artifact before installing. The expected APK signing certificate SHA-256 is `b6da01480eefd5fbf2cd3771b8d1021ec791304bdd6c4bf41d3faabad48ee5e1`.

## Build and use

Every APK and native library in CI is compiled from the source pinned in this repository. The [build workflow](.github/workflows/build.yml) fetches Termux's public key at a pinned commit, verifies its SHA-256, runs fauxdev's tests, builds the APK and four Termux-native libraries, compile-checks Studio on Linux, and verifies the APK certificate. CI uploads one ZIP artifact per architecture; each contains `camera2termux.apk`, `libfauxdev-termux.so`, and `camera2termux-run` at the root. No locally compiled library is checked in or repackaged.

For earlier published builds, use GitHub-signed Termux. Current source and its matching CI artifacts support the authenticated F-Droid Termux handoff on Android 14+. The APK contains ARM64, ARMv7, x86, and x86-64 native libraries. Download the artifact ZIP matching `dpkg --print-architecture` (`aarch64` uses `arm64`), extract it, then install the Termux files from the extracted directory:

```sh
install -m 755 libfauxdev-termux.so "$PREFIX/lib/libfauxdev-termux.so"
install -m 755 camera2termux-run "$PREFIX/bin/camera2termux-run"
install -Dm 444 camera2termux.apk "$PREFIX/share/camera2termux/camera2termux.apk"
```

Open Camera2Termux, grant camera permission, and tap **Start camera**. Then use `camera2termux-run COMMAND ...` for a native Termux program that opens `/dev/video0` through V4L2. The current launcher hands an already-connected FD to the command and preloads the native shim; earlier releases used an app-private socket path. The launcher uses the installed APK copy first because `pm path` can stall in Termux; keep it matched to the installed app and native library. `CAMERA2TERMUX_APK` can point to a matching APK elsewhere. The launcher does not enable the camera in arbitrary processes. Stop the service when finished.

To preview the stream with low buffering in Termux:X11, install the separate [Termux `ffplay` package](https://github.com/termux/termux-packages/blob/master/x11-packages/ffplay/build.sh), start Termux:X11 and the camera service, then run:

```sh
pkg install x11-repo
pkg install ffplay
export DISPLAY=:0  # use your Termux:X11 display if different
camera2termux-run ffplay -fflags nobuffer -framedrop -sync ext \
  -f video4linux2 -input_format rgb24 -video_size 320x240 -framerate 30 /dev/video0
```

The bridge's raw frames are sensor-oriented. For example, the tested Pixel's rear camera needs a clockwise turn in portrait mode:

```sh
camera2termux-run ffplay -fflags nobuffer -framedrop -sync ext \
  -f video4linux2 -input_format rgb24 -video_size 640x480 -framerate 30 \
  -vf transpose=clock /dev/video0
```

Use `transpose=cclock` if your image needs the opposite turn, and choose a size your camera exposes. These flags reduce startup buffering and allow late frames to be dropped; they do not increase the camera's capture rate.

To bypass the broker's YUV-to-RGB conversion, request NV12 explicitly with a matching current APK and fauxdev library:

```sh
camera2termux-run ffplay -fflags nobuffer -framedrop -sync ext \
  -f video4linux2 -input_format nv12 -video_size 640x480 -framerate 30 /dev/video0
```

NV12 still repacks Camera2's variable plane strides and copies frames across the bridge. Studio prefers NV12, scales and converts only its preview with libswscale, converts full resolution only for PNG photos, and sends NV12 directly to FFmpeg when recording. It falls back to RGB24 if NV12 streaming is unavailable.

PRoot/glibc programs need their own fauxdev build and setup; these Bionic libraries are for native Termux programs. The local F-Droid/Pixel result does not establish compatibility across devices, CPU architectures, or Termux variants.

## Studio (Termux:X11)

The experimental Linux desktop Studio uses the same `/dev/video0` stream for its live preview, PNG photos, and silent H.264 MP4 recording. Camera, NV12 capture size (RGB24 fallback), and fixed target FPS choices come from V4L2 enumeration. The on-screen capture FPS counts frames received and processed by Studio, not frames presented by Termux:X11. Photos and video use the full selected capture size; only the preview is scaled. Studio uses Camera2's reported sensor orientation for its portrait-default output and offers a manual quarter-turn correction that applies to preview, PNG, and MP4. Raw V4L2 frames remain in sensor orientation; Studio does not infer the physical phone rotation from Termux:X11. The Properties pane shows working V4L2 values and an available Camera2 report, not a complete Camera2 metadata browser.

Studio is currently **source-build-only in native Termux**. The APK/NDK CI artifact does not contain a locally compiled Studio binary. Install the Termux:X11 Android app, then install the build and runtime packages in native Termux from this checkout:

```sh
pkg install x11-repo
pkg install termux-x11-nightly qt6-qtbase qt6-qtdeclarative cmake clang pkg-config ffmpeg
pkg-config --modversion Qt6Quick libswscale
ffmpeg -hide_banner -encoders | grep libx264
bash scripts/build-studio-termux.sh
```

The version checks confirm Qt Quick, libswscale, and the H.264 encoder that Studio needs. If one fails, resolve that package before building. Start Camera2Termux's camera service in its Android app, then start the X server and Studio in Termux:

```sh
termux-x11 :0 &
export DISPLAY=:0
camera2termux-run camera2termux-studio
```

Open the Termux:X11 Android app to see the window. If the server is already running, use its existing display instead of starting another. Choose an existing writable save folder in Studio. MP4 is first written as a `.part` file and moved into place only after FFmpeg exits successfully. Recording drops whole frames if the encoder cannot keep up; the readout shows the drop count. On the tested Pixel 6a, 640×480 recording encoded about 11 frames/s while the camera delivered about 30 frames/s; a smaller size recorded more smoothly. No audio is recorded. Stop the Android camera service when finished.

For the tested Pixel 6a acceleration path, build [Mali_Termux-WSI](https://github.com/ZenFox3/Mali_Termux-WSI) in native Termux. Its `make vulkan` target creates `build/libvulkan.so.1`:

```sh
pkg install clang make git libxcb vulkan-headers
git clone https://github.com/ZenFox3/Mali_Termux-WSI.git "$HOME/Mali_Termux-WSI"
make -C "$HOME/Mali_Termux-WSI" vulkan
test -r "$HOME/Mali_Termux-WSI/build/libvulkan.so.1"
```

With the camera service and Termux:X11 running, launch Studio with its Vulkan library after `camera2termux-run` hands off the camera FD (Android's `app_process` strips `LD_*` variables before that handoff):

```sh
export DISPLAY=:0
camera2termux-run env \
  LD_LIBRARY_PATH="$HOME/Mali_Termux-WSI/build:$PREFIX/lib" \
  LD_PRELOAD="/system/lib64/libbinder_ndk.so:$PREFIX/lib/libfauxdev-termux.so" \
  QT_QPA_PLATFORM=xcb QSG_RHI_BACKEND=vulkan camera2termux-studio
```

The WSI library accelerates Qt's Vulkan window presentation, while Studio still reads the camera through fauxdev and converts NV12 for the preview. This is specific to that installed Mali WSI stack. Check visible output and the active renderer before treating it as a working hardware path.

## OpenCV object detection (Termux:X11)

The [OpenCV example](examples/opencv/) reads 640×480 NV12 from fauxdev, rotates the tested Pixel 6a rear camera clockwise, and draws detections from a 320×320 YOLOv8n ONNX model. It recognizes the model's 80 COCO classes. Inference uses OpenCV's CPU DNN engine; Mali WSI or XFCE can display its window but do not accelerate the model. Only one client can stream the camera at a time, so close Studio first.

Download the [fixed 320×320 YOLOv8n ONNX export](https://github.com/siromermer/object-detection-benchmark/blob/6e4670fc439888764b71e6b243842468948f4e37/models/yolov8n_320.onnx) directly in Termux. Check its SHA-256 before using it:

```sh
pkg install curl
mkdir -p "$HOME/.cache/camera2termux"
curl -fL --retry 2 \
  'https://raw.githubusercontent.com/siromermer/object-detection-benchmark/6e4670fc439888764b71e6b243842468948f4e37/models/yolov8n_320.onnx' \
  -o "$HOME/.cache/camera2termux/yolov8n-320.onnx"
echo 'c18257b26cb4231e6318343522044cdd9083d1ac2ac29bcd5027f0ea1f236696  '"$HOME/.cache/camera2termux/yolov8n-320.onnx" | sha256sum -c -
```

From this checkout in native Termux, start Camera2Termux's camera service and Termux:X11, then build and launch the example:

```sh
pkg install x11-repo
pkg install clang opencv
mkdir -p build/opencv
clang -D_GNU_SOURCE -O2 -std=c11 examples/opencv/capture_nv12.c -o build/opencv/capture-nv12
clang++ -O2 -std=c++17 examples/opencv/detect_objects.cpp \
  $(pkg-config --cflags --libs opencv5) -o build/opencv/detect-objects
export DISPLAY=:0
camera2termux-run "$PWD/build/opencv/capture-nv12" | \
  "$PWD/build/opencv/detect-objects" "$HOME/.cache/camera2termux/yolov8n-320.onnx" \
  "$PWD/examples/opencv/coco.names"
```

Press Esc in the OpenCV window to exit. On another camera, change the fixed size or rotation in the example to match its supported modes and sensor orientation. The broker still copies and packs NV12, and OpenCV converts each frame to BGR for this model. Smaller model input improved the tested Pixel's live detector from about 1.6 to 8 processed frames/s, with reduced small-object accuracy; these are not display FPS measurements.
