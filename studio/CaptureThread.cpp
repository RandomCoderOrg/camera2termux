#include "CameraEngine.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QCoreApplication>
#include <QEventLoop>
#include <QSaveFile>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTransform>
#include <QDebug>

#include <cerrno>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

extern "C" {
#include <libswscale/swscale.h>
}

namespace {
struct Buffer { void *data = MAP_FAILED; size_t length = 0; };

int xioctl(int fd, unsigned long request, void *argument) {
    int result;
    do result = ioctl(fd, request, argument); while (result < 0 && errno == EINTR);
    return result;
}

QString systemError(const QString &operation) {
    return operation + QStringLiteral(": ") + QString::fromLocal8Bit(strerror(errno));
}

struct Device {
    int fd = -1;
    bool streaming = false;
    QSize size;
    uint32_t pixelFormat = V4L2_PIX_FMT_RGB24;
    size_t frameBytes = 0;
    SwsContext *previewScaler = nullptr;
    int targetFps = 0;
    QStringList cameras, sizes, rates;
    int cameraIndex = 0, sizeIndex = 0, fpsIndex = -1;
    QString properties;
    std::vector<Buffer> buffers;

    ~Device() { closeDevice(); }
    void closeDevice() {
        sws_freeContext(previewScaler);
        previewScaler = nullptr;
        if (fd < 0) return;
        if (streaming) {
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(fd, VIDIOC_STREAMOFF, &type);
        }
        streaming = false;
        for (Buffer &buffer : buffers) {
            if (buffer.data != MAP_FAILED) munmap(buffer.data, buffer.length);
        }
        buffers.clear();
        struct v4l2_requestbuffers release = {};
        release.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        release.memory = V4L2_MEMORY_MMAP;
        xioctl(fd, VIDIOC_REQBUFS, &release);
        close(fd);
        fd = -1;
    }
};

QString fourcc(uint32_t value) {
    char bytes[5] = {char(value), char(value >> 8), char(value >> 16), char(value >> 24), 0};
    return QString::fromLatin1(bytes);
}

QString cString(const uint8_t *value, size_t capacity) {
    return QString::fromLocal8Bit(reinterpret_cast<const char *>(value),
                                  int(strnlen(reinterpret_cast<const char *>(value), capacity)));
}

QImage convertNv12(const uchar *pixels, QSize source, QSize target, SwsContext *scaler) {
    QImage image(target, QImage::Format_RGB888);
    if (image.isNull()) return {};
    const uint8_t *input[] = {pixels, pixels + size_t(source.width()) * source.height(), nullptr, nullptr};
    const int inputStride[] = {source.width(), source.width(), 0, 0};
    uint8_t *output[] = {image.bits(), nullptr, nullptr, nullptr};
    const int outputStride[] = {int(image.bytesPerLine()), 0, 0, 0};
    return sws_scale(scaler, input, inputStride, 0, source.height(), output, outputStride) == target.height()
               ? image : QImage();
}

bool configure(Device &device, int wantedCamera, int wantedSize, int wantedFps,
               bool preferNv12, QString &error) {
    device.cameras.clear();
    device.sizes.clear();
    device.rates.clear();
    device.fd = open("/dev/video0", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (device.fd < 0) { error = systemError(QStringLiteral("Open /dev/video0")); return false; }
    struct v4l2_capability capability = {};
    if (xioctl(device.fd, VIDIOC_QUERYCAP, &capability) < 0) {
        error = systemError(QStringLiteral("Query V4L2 capabilities")); return false;
    }
    uint32_t caps = capability.capabilities & V4L2_CAP_DEVICE_CAPS ?
                    capability.device_caps : capability.capabilities;
    if (!(caps & V4L2_CAP_VIDEO_CAPTURE) || !(caps & V4L2_CAP_STREAMING)) {
        error = QStringLiteral("Camera does not expose V4L2 MMAP capture"); return false;
    }
    for (unsigned int i = 0; i < 64; ++i) {
        struct v4l2_input input = {};
        input.index = i;
        if (xioctl(device.fd, VIDIOC_ENUMINPUT, &input) < 0) {
            if (errno != EINVAL) error = systemError(QStringLiteral("Enumerate cameras"));
            break;
        }
        QString name = cString(input.name, sizeof(input.name));
        device.cameras.append(name.isEmpty() ? QStringLiteral("Camera %1").arg(i) : name);
    }
    if (!error.isEmpty()) return false;
    if (device.cameras.isEmpty()) { error = QStringLiteral("No Camera2 inputs are available"); return false; }
    device.cameraIndex = qBound(0, wantedCamera, device.cameras.size() - 1);
    int input = device.cameraIndex;
    if (xioctl(device.fd, VIDIOC_S_INPUT, &input) < 0) {
        error = systemError(QStringLiteral("Select camera")); return false;
    }
    std::vector<QSize> modes;
    for (uint32_t pixelFormat : {V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_RGB24}) {
        if (!preferNv12 && pixelFormat == V4L2_PIX_FMT_NV12) continue;
        for (unsigned int i = 0; i < 256; ++i) {
            struct v4l2_frmsizeenum item = {};
            item.index = i;
            item.pixel_format = pixelFormat;
            if (xioctl(device.fd, VIDIOC_ENUM_FRAMESIZES, &item) < 0) {
                if (errno != EINVAL) error = systemError(QStringLiteral("Enumerate sizes"));
                break;
            }
            if (item.type != V4L2_FRMSIZE_TYPE_DISCRETE) continue;
            QSize size(int(item.discrete.width), int(item.discrete.height));
            if (!size.isValid() || size.width() > 8192 || size.height() > 8192) continue;
            modes.push_back(size);
            device.sizes.append(QStringLiteral("%1 × %2").arg(size.width()).arg(size.height()));
        }
        if (!error.isEmpty() || !modes.empty()) {
            device.pixelFormat = pixelFormat;
            break;
        }
    }
    if (!error.isEmpty()) return false;
    if (modes.empty()) { error = QStringLiteral("No NV12 or RGB24 capture sizes are exposed"); return false; }
    if (wantedSize < 0) {
        wantedSize = 0;
        for (size_t i = 0; i < modes.size(); ++i)
            if (modes[i] == QSize(640, 480)) { wantedSize = int(i); break; }
    }
    device.sizeIndex = qBound(0, wantedSize, int(modes.size()) - 1);
    device.size = modes[size_t(device.sizeIndex)];
    struct v4l2_format format = {};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = uint32_t(device.size.width());
    format.fmt.pix.height = uint32_t(device.size.height());
    format.fmt.pix.pixelformat = device.pixelFormat;
    if (xioctl(device.fd, VIDIOC_S_FMT, &format) < 0) {
        error = systemError(QStringLiteral("Set capture size")); return false;
    }
    device.frameBytes = size_t(device.size.width()) * device.size.height() *
                        (device.pixelFormat == V4L2_PIX_FMT_NV12 ? 3 : 6) / 2;
    if (format.fmt.pix.pixelformat != device.pixelFormat ||
        format.fmt.pix.width != uint32_t(device.size.width()) ||
        format.fmt.pix.height != uint32_t(device.size.height()) ||
        format.fmt.pix.bytesperline != format.fmt.pix.width *
                                     (device.pixelFormat == V4L2_PIX_FMT_NV12 ? 1u : 3u) ||
        format.fmt.pix.sizeimage != device.frameBytes) {
        error = QStringLiteral("Camera returned an unsupported buffer layout"); return false;
    }
    if (device.pixelFormat == V4L2_PIX_FMT_NV12) {
        QSize preview = device.size.scaled(QSize(640, 640), Qt::KeepAspectRatio);
        device.previewScaler = sws_getContext(device.size.width(), device.size.height(), AV_PIX_FMT_NV12,
                                              preview.width(), preview.height(), AV_PIX_FMT_RGB24,
                                              SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        if (!device.previewScaler) {
            error = QStringLiteral("Could not prepare NV12 preview conversion"); return false;
        }
    }

    std::vector<int> rates;
    for (unsigned int i = 0; i < 256; ++i) {
        struct v4l2_frmivalenum item = {};
        item.index = i;
        item.pixel_format = device.pixelFormat;
        item.width = format.fmt.pix.width;
        item.height = format.fmt.pix.height;
        if (xioctl(device.fd, VIDIOC_ENUM_FRAMEINTERVALS, &item) < 0) {
            if (errno != EINVAL && errno != ENOTTY)
                error = systemError(QStringLiteral("Enumerate frame rates"));
            break;
        }
        if (item.type != V4L2_FRMIVAL_TYPE_DISCRETE ||
            item.discrete.numerator != 1 || !item.discrete.denominator) continue;
        rates.push_back(int(item.discrete.denominator));
        device.rates.append(QStringLiteral("%1 fps").arg(item.discrete.denominator));
    }
    if (!error.isEmpty()) return false;
    if (wantedFps >= 0 && !rates.empty()) {
        device.fpsIndex = qBound(0, wantedFps, int(rates.size()) - 1);
        struct v4l2_streamparm parm = {};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = uint32_t(rates[size_t(device.fpsIndex)]);
        if (xioctl(device.fd, VIDIOC_S_PARM, &parm) < 0) {
            error = systemError(QStringLiteral("Set frame rate")); return false;
        }
        device.targetFps = int(parm.parm.capture.timeperframe.denominator);
    } else {
        struct v4l2_streamparm parm = {};
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(device.fd, VIDIOC_G_PARM, &parm) == 0 &&
            parm.parm.capture.timeperframe.numerator == 1)
            device.targetFps = int(parm.parm.capture.timeperframe.denominator);
    }
    for (size_t i = 0; i < rates.size(); ++i)
        if (rates[i] == device.targetFps) device.fpsIndex = int(i);

    struct v4l2_requestbuffers request = {};
    request.count = 3;
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request.memory = V4L2_MEMORY_MMAP;
    if (xioctl(device.fd, VIDIOC_REQBUFS, &request) < 0) {
        error = systemError(QStringLiteral("Allocate capture buffers")); return false;
    }
    if (request.count < 2 || request.count > 64) {
        error = QStringLiteral("Camera returned an invalid buffer count"); return false;
    }
    for (unsigned int i = 0; i < request.count; ++i) {
        struct v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (xioctl(device.fd, VIDIOC_QUERYBUF, &buffer) < 0) {
            error = systemError(QStringLiteral("Query capture buffer")); return false;
        }
        if (buffer.length < format.fmt.pix.sizeimage || buffer.length > 64u * 1024u * 1024u) {
            error = QStringLiteral("Camera returned an invalid buffer length"); return false;
        }
        void *mapping = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, device.fd, buffer.m.offset);
        if (mapping == MAP_FAILED) {
            error = systemError(QStringLiteral("Map capture buffer")); return false;
        }
        device.buffers.push_back({mapping, buffer.length});
        if (xioctl(device.fd, VIDIOC_QBUF, &buffer) < 0) {
            error = systemError(QStringLiteral("Queue capture buffer")); return false;
        }
    }
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(device.fd, VIDIOC_STREAMON, &type) < 0) {
        if (preferNv12 && device.pixelFormat == V4L2_PIX_FMT_NV12 && errno == ENOTTY) {
            device.closeDevice();
            error.clear();
            return configure(device, wantedCamera, wantedSize, wantedFps, false, error);
        }
        error = systemError(QStringLiteral("Start camera stream")); return false;
    }
    device.streaming = true;
    device.properties = QStringLiteral("Driver: %1\nCard: %2\nBus: %3\nInput: %4 (%5)\nFormat: %6\nSize: %7 × %8\nBuffer stride: %9 bytes\nBuffer count: %10\nCapture: MMAP streaming\nFrame rate control: %11")
        .arg(cString(capability.driver, sizeof(capability.driver)),
             cString(capability.card, sizeof(capability.card)),
             cString(capability.bus_info, sizeof(capability.bus_info)))
        .arg(device.cameraIndex).arg(device.cameras[device.cameraIndex])
        .arg(fourcc(format.fmt.pix.pixelformat))
        .arg(device.size.width()).arg(device.size.height())
        .arg(format.fmt.pix.bytesperline).arg(request.count)
        .arg(device.targetFps ? QStringLiteral("%1 fps target").arg(device.targetFps)
                              : QStringLiteral("not exposed"));
    return true;
}

bool writePhoto(const QString &path, const QImage &image, QString &error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
    if (!image.save(&file, "PNG") || !file.commit()) {
        error = file.errorString().isEmpty() ? QStringLiteral("PNG encoding failed") : file.errorString();
        return false;
    }
    return true;
}
}

CaptureThread::CaptureThread(QObject *parent) : QThread(parent) {}

void CaptureThread::enqueue(Command command) {
    QMutexLocker lock(&m_mutex);
    m_commands.enqueue(std::move(command));
}
void CaptureThread::chooseCamera(int index) { enqueue({CommandType::Camera, index, {}, {}}); }
void CaptureThread::chooseSize(int index) { enqueue({CommandType::Size, index, {}, {}}); }
void CaptureThread::chooseFps(int index) { enqueue({CommandType::Fps, index, {}, {}}); }
void CaptureThread::takePhoto(const QString &path) { enqueue({CommandType::Photo, -1, path, {}}); }
void CaptureThread::startRecording(const QString &path) { enqueue({CommandType::Record, -1, path, {}}); }
void CaptureThread::stopRecording() { enqueue({CommandType::StopRecord, -1, {}, {}}); }
void CaptureThread::setOrientations(const QHash<QString, int> &orientations) {
    Command command{CommandType::Orientations, -1, {}, {}};
    command.orientations = orientations;
    enqueue(std::move(command));
}
void CaptureThread::setExtraRotation(int degrees) {
    enqueue({CommandType::Rotation, degrees, {}, {}});
}

void CaptureThread::run() {
    Device device;
    QProcess recorder;
    recorder.setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment encoderEnvironment = QProcessEnvironment::systemEnvironment();
    encoderEnvironment.remove(QStringLiteral("LD_PRELOAD"));
    encoderEnvironment.remove(QStringLiteral("FAUXDEV_VIDEO_FD"));
    encoderEnvironment.remove(QStringLiteral("FAUXDEV_VIDEO_SOCKET"));
    recorder.setProcessEnvironment(encoderEnvironment);
    QString recordFinal, recordPart, pendingPhoto, pendingRecord;
    int cameraIndex = 0, sizeIndex = -1, fpsIndex = -1, dropped = 0;
    int extraRotation = 0, outputRotation = 0;
    QHash<QString, int> orientations;
    bool reconfigure = true;
    QElapsedTimer clock;
    clock.start();
    std::deque<qint64> arrivals;
    qint64 lastMetricMs = 0;

    auto finishRecording = [&] {
        if (recordFinal.isEmpty()) return;
        recorder.closeWriteChannel();
        if (!recorder.waitForFinished(15000)) {
            recorder.kill();
            recorder.waitForFinished(2000);
        }
        bool okay = recorder.exitStatus() == QProcess::NormalExit && recorder.exitCode() == 0;
        QString output = QString::fromLocal8Bit(recorder.readAll()).trimmed();
        if (!okay) qWarning() << "FFmpeg recording failed" << recorder.exitStatus()
                              << recorder.exitCode() << output.right(1200);
        if (okay && (QFile::exists(recordFinal) || !QFile::rename(recordPart, recordFinal))) {
            okay = false;
            output = QStringLiteral("Could not move completed MP4 to the selected folder");
        }
        if (!okay) QFile::remove(recordPart);
        emit recordingChanged(false, dropped);
        emit statusChanged(okay ? QStringLiteral("Saved video: %1").arg(recordFinal) :
                           QStringLiteral("Video was not saved: %1").arg(output.left(400)), !okay);
        recordFinal.clear();
        recordPart.clear();
    };

    auto beginRecording = [&](const QString &path) {
        if (!device.streaming || !recordFinal.isEmpty()) return;
        recordPart = path + QStringLiteral(".part");
        if (QFile::exists(path) || QFile::exists(recordPart)) {
            emit statusChanged(QStringLiteral("Video path already exists"), true);
            recordPart.clear();
            return;
        }
        int fps = device.targetFps ? device.targetFps : 30;
        QStringList filter;
        if (outputRotation == 90) filter = {QStringLiteral("-vf"), QStringLiteral("transpose=clock")};
        else if (outputRotation == 180) filter = {QStringLiteral("-vf"), QStringLiteral("hflip,vflip")};
        else if (outputRotation == 270) filter = {QStringLiteral("-vf"), QStringLiteral("transpose=cclock")};
        QStringList args = {QStringLiteral("-nostdin"), QStringLiteral("-hide_banner"),
            QStringLiteral("-loglevel"), QStringLiteral("error"), QStringLiteral("-n"),
            QStringLiteral("-f"), QStringLiteral("rawvideo"), QStringLiteral("-pixel_format"),
            device.pixelFormat == V4L2_PIX_FMT_NV12 ? QStringLiteral("nv12") : QStringLiteral("rgb24"),
            QStringLiteral("-video_size"),
            QStringLiteral("%1x%2").arg(device.size.width()).arg(device.size.height()),
            QStringLiteral("-framerate"), QString::number(fps),
            QStringLiteral("-use_wallclock_as_timestamps"), QStringLiteral("1"),
            QStringLiteral("-i"),
            QStringLiteral("pipe:0"), QStringLiteral("-an")};
        args += filter;
        args += {QStringLiteral("-c:v"), QStringLiteral("libx264"),
                 QStringLiteral("-preset"), QStringLiteral("ultrafast"),
                 QStringLiteral("-crf"), QStringLiteral("23"),
                 QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), QStringLiteral("-movflags"),
            QStringLiteral("+faststart"), QStringLiteral("-fps_mode"), QStringLiteral("vfr"),
            QStringLiteral("-f"), QStringLiteral("mp4"), recordPart};
        qInfo() << "FFmpeg recording command" << QStringLiteral("ffmpeg") << args;
        recorder.start(QStringLiteral("ffmpeg"), args, QIODevice::ReadWrite);
        if (!recorder.waitForStarted(5000)) {
            emit statusChanged(QStringLiteral("Could not start FFmpeg: %1").arg(recorder.errorString()), true);
            recordPart.clear();
            return;
        }
        recordFinal = path;
        dropped = 0;
        emit recordingChanged(true, 0);
        emit statusChanged(QStringLiteral("Recording silently to %1").arg(path), false);
    };

    while (!isInterruptionRequested()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        QQueue<Command> commands;
        {
            QMutexLocker lock(&m_mutex);
            commands.swap(m_commands);
        }
        while (!commands.isEmpty()) {
            Command command = commands.dequeue();
            switch (command.type) {
            case CommandType::Camera:
                cameraIndex = command.index; sizeIndex = -1; fpsIndex = -1; reconfigure = true; break;
            case CommandType::Size:
                sizeIndex = command.index; fpsIndex = -1; reconfigure = true; break;
            case CommandType::Fps:
                fpsIndex = command.index; reconfigure = true; break;
            case CommandType::Photo: pendingPhoto = command.path; break;
            case CommandType::Record: pendingRecord = command.path; break;
            case CommandType::StopRecord: pendingRecord.clear(); finishRecording(); break;
            case CommandType::Orientations:
                orientations = std::move(command.orientations);
                break;
            case CommandType::Rotation:
                if (extraRotation != command.index) finishRecording();
                extraRotation = command.index;
                break;
            }
        }
        if (reconfigure) {
            finishRecording();
            device.closeDevice();
            emit statusChanged(QStringLiteral("Configuring camera…"), false);
            QString error;
            if (!configure(device, cameraIndex, sizeIndex, fpsIndex, true, error)) {
                device.closeDevice();
                emit statusChanged(error, true);
            } else {
                cameraIndex = device.cameraIndex;
                sizeIndex = device.sizeIndex;
                fpsIndex = device.fpsIndex;
                emit configured(device.cameras, device.sizes, device.rates,
                                cameraIndex, sizeIndex, fpsIndex, device.properties,
                                device.targetFps);
                emit statusChanged(QStringLiteral("Live · %1 × %2")
                                   .arg(device.size.width()).arg(device.size.height()), false);
                arrivals.clear();
                emit deliveredFpsChanged(0);
            }
            reconfigure = false;
        }
        if (!device.streaming) { msleep(100); continue; }
        const QString cameraName = device.cameras.value(device.cameraIndex);
        const QRegularExpression idPattern(QStringLiteral("(\\S+)$"));
        const auto idMatch = idPattern.match(cameraName);
        int sensorRotation = orientations.value(idMatch.hasMatch() ? idMatch.captured(1) : QString(), 0);
        int newRotation = (sensorRotation + extraRotation) % 360;
        if (newRotation != outputRotation) finishRecording();
        outputRotation = newRotation;
        if (!pendingRecord.isEmpty()) {
            beginRecording(pendingRecord);
            pendingRecord.clear();
        }
        struct pollfd ready = {device.fd, POLLIN, 0};
        int polled = poll(&ready, 1, 100);
        if (polled < 0 && errno == EINTR) continue;
        if (polled < 0 || (ready.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            emit statusChanged(polled < 0 ? systemError(QStringLiteral("Wait for frame")) :
                               QStringLiteral("Camera stream disconnected"), true);
            finishRecording();
            device.closeDevice();
            continue;
        }
        if (!polled || !(ready.revents & POLLIN)) continue;
        struct v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (xioctl(device.fd, VIDIOC_DQBUF, &buffer) < 0) {
            if (errno == EAGAIN) continue;
            emit statusChanged(systemError(QStringLiteral("Read camera frame")), true);
            finishRecording();
            device.closeDevice();
            continue;
        }
        const size_t frameBytes = device.frameBytes;
        if (buffer.index >= device.buffers.size() || buffer.bytesused < frameBytes ||
            frameBytes > device.buffers[buffer.index].length) {
            emit statusChanged(QStringLiteral("Camera returned an invalid frame"), true);
            finishRecording();
            device.closeDevice();
            continue;
        }
        const uchar *pixels = static_cast<const uchar *>(device.buffers[buffer.index].data);
        QImage full;
        if (device.pixelFormat == V4L2_PIX_FMT_RGB24)
            full = QImage(pixels, device.size.width(), device.size.height(),
                          device.size.width() * 3, QImage::Format_RGB888);
        if (!pendingPhoto.isEmpty()) {
            QString error;
            QImage photo = full;
            if (device.pixelFormat == V4L2_PIX_FMT_NV12) {
                SwsContext *scaler = sws_getContext(device.size.width(), device.size.height(),
                                                    AV_PIX_FMT_NV12, device.size.width(),
                                                    device.size.height(), AV_PIX_FMT_RGB24,
                                                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                if (scaler) photo = convertNv12(pixels, device.size, device.size, scaler);
                sws_freeContext(scaler);
            }
            if (photo.isNull()) error = QStringLiteral("Could not convert camera frame");
            else {
                if (outputRotation) photo = photo.transformed(QTransform().rotate(outputRotation));
                (void)writePhoto(pendingPhoto, photo, error);
            }
            if (error.isEmpty()) emit photoSaved(pendingPhoto);
            else emit statusChanged(QStringLiteral("Photo was not saved: %1").arg(error), true);
            pendingPhoto.clear();
        }
        if (!recordFinal.isEmpty()) {
            if (recorder.state() != QProcess::Running) {
                finishRecording();
            } else {
                if (recorder.bytesToWrite()) recorder.waitForBytesWritten(20);
            }
            if (!recordFinal.isEmpty() && recorder.bytesToWrite() > qint64(frameBytes)) {
                ++dropped;
                emit recordingChanged(true, dropped);
            } else if (!recordFinal.isEmpty() &&
                       recorder.write(reinterpret_cast<const char *>(pixels), qint64(frameBytes)) !=
                       qint64(frameBytes)) {
                emit statusChanged(QStringLiteral("FFmpeg input write failed"), true);
                finishRecording();
            }
        }
        QImage preview = device.pixelFormat == V4L2_PIX_FMT_NV12 ?
            convertNv12(pixels, device.size, device.size.scaled(QSize(640, 640), Qt::KeepAspectRatio),
                        device.previewScaler) :
            full.scaled(QSize(640, 640), Qt::KeepAspectRatio, Qt::FastTransformation);
        if (preview.isNull()) {
            emit statusChanged(QStringLiteral("Could not convert camera preview"), true);
            finishRecording();
            device.closeDevice();
            continue;
        }
        if (outputRotation) preview = preview.transformed(QTransform().rotate(outputRotation));
        emit frameReady(preview);
        qint64 nowNs = clock.nsecsElapsed();
        arrivals.push_back(nowNs);
        if (arrivals.size() > 31) arrivals.pop_front();
        if (clock.elapsed() - lastMetricMs >= 500 && arrivals.size() >= 2) {
            double fps = double(arrivals.size() - 1) * 1000000000.0 /
                         double(arrivals.back() - arrivals.front());
            emit deliveredFpsChanged(fps);
            lastMetricMs = clock.elapsed();
        }
        if (xioctl(device.fd, VIDIOC_QBUF, &buffer) < 0) {
            emit statusChanged(systemError(QStringLiteral("Requeue camera frame")), true);
            finishRecording();
            device.closeDevice();
        }
    }
    finishRecording();
    device.closeDevice();
}
