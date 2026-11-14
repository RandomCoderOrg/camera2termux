#include "CameraEngine.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

CameraEngine::CameraEngine(QObject *parent) : QObject(parent), m_saveFolder(QDir::homePath()) {
    connect(&m_capture, &CaptureThread::configured, this,
            [this](const QStringList &cameras, const QStringList &sizes,
                   const QStringList &rates, int cameraIndex, int sizeIndex,
                   int fpsIndex, const QString &properties, int targetFps) {
        m_cameras = cameras; m_sizes = sizes; m_rates = rates;
        m_cameraIndex = cameraIndex; m_sizeIndex = sizeIndex;
        m_fpsIndex = fpsIndex; m_targetFps = targetFps;
        m_properties = properties;
        emit optionsChanged();
        emit propertiesChanged();
    });
    connect(&m_capture, &CaptureThread::frameReady, this, &CameraEngine::frameReady);
    connect(&m_capture, &CaptureThread::deliveredFpsChanged, this, [this](double fps) {
        m_deliveredFps = fps;
        emit deliveredFpsChanged();
    });
    connect(&m_capture, &CaptureThread::recordingChanged, this,
            [this](bool active, int dropped) {
        m_recording = active; m_droppedFrames = dropped;
        emit recordingChanged();
    });
    connect(&m_capture, &CaptureThread::statusChanged, this,
            [this](const QString &status, bool error) {
        m_status = status; m_hasError = error;
        emit statusChanged();
    });
    connect(&m_capture, &CaptureThread::photoSaved, this, [this](const QString &path) {
        m_status = QStringLiteral("Saved photo: %1").arg(path);
        m_hasError = false;
        emit statusChanged();
    });
    connect(&m_reportProcess, &QProcess::errorOccurred, this, [this] {
        m_camera2Report = QStringLiteral("Camera2 report could not start: %1")
            .arg(m_reportProcess.errorString());
        emit camera2ReportChanged();
        if (!m_captureStarted) { m_captureStarted = true; m_capture.start(); }
    });
    connect(&m_reportProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus exit) {
        const QString output = QString::fromUtf8(m_reportProcess.readAllStandardOutput());
        const QString error = QString::fromUtf8(m_reportProcess.readAllStandardError());
        m_camera2Report = exit == QProcess::NormalExit && code == 0 ? output :
            QStringLiteral("Camera2 report unavailable: %1").arg(error.trimmed());
        emit camera2ReportChanged();
        if (exit == QProcess::NormalExit && code == 0) {
            QHash<QString, int> orientations;
            const QRegularExpression cameraPattern(QStringLiteral("^Camera (\\S+)$"));
            const QRegularExpression rotationPattern(QStringLiteral("^\\s+sensor orientation degrees: (0|90|180|270)$"));
            QString currentId;
            for (const QString &line : output.split(QLatin1Char('\n'))) {
                const auto camera = cameraPattern.match(line);
                if (camera.hasMatch()) currentId = camera.captured(1);
                const auto rotation = rotationPattern.match(line);
                if (!currentId.isEmpty() && rotation.hasMatch())
                    orientations.insert(currentId, rotation.captured(1).toInt());
            }
            m_capture.setOrientations(orientations);
        }
        if (!m_captureStarted) { m_captureStarted = true; m_capture.start(); }
    });
    refreshCamera2Report();
    QTimer::singleShot(10000, this, [this] {
        if (m_captureStarted) return;
        if (m_reportProcess.state() != QProcess::NotRunning) m_reportProcess.kill();
        m_captureStarted = true;
        m_capture.start();
    });
}

CameraEngine::~CameraEngine() {
    if (m_reportProcess.state() != QProcess::NotRunning) {
        m_reportProcess.kill();
        m_reportProcess.waitForFinished(2000);
    }
    if (m_captureStarted) {
        m_capture.requestInterruption();
        m_capture.wait();
    }
}

void CameraEngine::chooseCamera(int index) {
    if (index >= 0 && index < m_cameras.size()) m_capture.chooseCamera(index);
}
void CameraEngine::chooseSize(int index) {
    if (index >= 0 && index < m_sizes.size()) m_capture.chooseSize(index);
}
void CameraEngine::chooseFps(int index) {
    if (index >= 0 && index < m_rates.size()) m_capture.chooseFps(index);
}

QString CameraEngine::nextPath(const QString &extension) const {
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss-zzz"));
    QDir folder(m_saveFolder);
    for (int suffix = 0; suffix < 1000; ++suffix) {
        QString name = QStringLiteral("camera2termux-%1").arg(stamp);
        if (suffix) name += QStringLiteral("-%1").arg(suffix);
        const QString path = folder.filePath(name + QLatin1Char('.') + extension);
        if (!QFile::exists(path) && !QFile::exists(path + QStringLiteral(".part"))) return path;
    }
    return {};
}

void CameraEngine::takePhoto() {
    const QString path = nextPath(QStringLiteral("png"));
    if (path.isEmpty()) {
        m_status = QStringLiteral("Could not choose a unique photo path");
        m_hasError = true; emit statusChanged(); return;
    }
    m_capture.takePhoto(path);
}

void CameraEngine::toggleRecording() {
    if (m_recording) { m_capture.stopRecording(); return; }
    const QString path = nextPath(QStringLiteral("mp4"));
    if (path.isEmpty()) {
        m_status = QStringLiteral("Could not choose a unique video path");
        m_hasError = true; emit statusChanged(); return;
    }
    m_capture.startRecording(path);
}

void CameraEngine::setSaveFolder(const QString &path) {
    QDir folder(QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path);
    QFileInfo info(folder.absolutePath());
    if (!info.isDir() || !info.isWritable()) {
        m_status = QStringLiteral("Choose an existing writable folder");
        m_hasError = true; emit statusChanged(); return;
    }
    m_saveFolder = folder.absolutePath();
    emit saveFolderChanged();
}

void CameraEngine::reconnect() {
    if (!QProcess::startDetached(QStringLiteral("camera2termux-run"),
                                 {QStringLiteral("camera2termux-studio")})) {
        m_status = QStringLiteral("Reconnect failed. Restart Studio with camera2termux-run.");
        m_hasError = true;
        emit statusChanged();
        return;
    }
    QCoreApplication::quit();
}

void CameraEngine::chooseRotation(int index) {
    if (index < 0 || index > 3 || m_rotationIndex == index) return;
    m_rotationIndex = index;
    m_capture.setExtraRotation(index * 90);
    emit rotationChanged();
}

void CameraEngine::refreshCamera2Report() {
    if (m_reportProcess.state() != QProcess::NotRunning) return;
    m_camera2Report = QStringLiteral("Loading the available Camera2 report…");
    emit camera2ReportChanged();
    m_reportProcess.start(QStringLiteral("camera2termux-run"),
                          {QStringLiteral("fauxdev-camera-info")});
}

QString CameraEngine::filterText(const QString &text, const QString &query) const {
    if (query.trimmed().isEmpty()) return text;
    QStringList matching;
    for (const QString &line : text.split(QLatin1Char('\n')))
        if (line.contains(query, Qt::CaseInsensitive)) matching.append(line);
    return matching.join(QLatin1Char('\n'));
}
