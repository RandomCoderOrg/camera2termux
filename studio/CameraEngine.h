#pragma once

#include <QImage>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QSize>
#include <QThread>

class CaptureThread final : public QThread {
    Q_OBJECT
public:
    explicit CaptureThread(QObject *parent = nullptr);
    void chooseCamera(int index);
    void chooseSize(int index);
    void chooseFps(int index);
    void takePhoto(const QString &path);
    void startRecording(const QString &path);
    void stopRecording();
    void setOrientations(const QHash<QString, int> &orientations);
    void setExtraRotation(int degrees);

signals:
    void configured(const QStringList &cameras, const QStringList &sizes,
                    const QStringList &rates, int cameraIndex, int sizeIndex,
                    int fpsIndex, const QString &properties, int targetFps);
    void frameReady(const QImage &frame);
    void deliveredFpsChanged(double fps);
    void recordingChanged(bool active, int dropped);
    void statusChanged(const QString &status, bool error);
    void photoSaved(const QString &path);

protected:
    void run() override;

private:
    enum class CommandType { Camera, Size, Fps, Photo, Record, StopRecord, Orientations, Rotation };
    struct Command {
        CommandType type;
        int index = -1;
        QString path;
        QHash<QString, int> orientations;
    };
    void enqueue(Command command);
    QMutex m_mutex;
    QQueue<Command> m_commands;
};

class CameraEngine final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList cameras READ cameras NOTIFY optionsChanged)
    Q_PROPERTY(QStringList sizes READ sizes NOTIFY optionsChanged)
    Q_PROPERTY(QStringList rates READ rates NOTIFY optionsChanged)
    Q_PROPERTY(int cameraIndex READ cameraIndex NOTIFY optionsChanged)
    Q_PROPERTY(int sizeIndex READ sizeIndex NOTIFY optionsChanged)
    Q_PROPERTY(int fpsIndex READ fpsIndex NOTIFY optionsChanged)
    Q_PROPERTY(int targetFps READ targetFps NOTIFY optionsChanged)
    Q_PROPERTY(double deliveredFps READ deliveredFps NOTIFY deliveredFpsChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(int droppedFrames READ droppedFrames NOTIFY recordingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY statusChanged)
    Q_PROPERTY(QString saveFolder READ saveFolder NOTIFY saveFolderChanged)
    Q_PROPERTY(QString properties READ properties NOTIFY propertiesChanged)
    Q_PROPERTY(QString camera2Report READ camera2Report NOTIFY camera2ReportChanged)
    Q_PROPERTY(int rotationIndex READ rotationIndex NOTIFY rotationChanged)

public:
    explicit CameraEngine(QObject *parent = nullptr);
    ~CameraEngine() override;

    QStringList cameras() const { return m_cameras; }
    QStringList sizes() const { return m_sizes; }
    QStringList rates() const { return m_rates; }
    int cameraIndex() const { return m_cameraIndex; }
    int sizeIndex() const { return m_sizeIndex; }
    int fpsIndex() const { return m_fpsIndex; }
    int targetFps() const { return m_targetFps; }
    double deliveredFps() const { return m_deliveredFps; }
    bool recording() const { return m_recording; }
    int droppedFrames() const { return m_droppedFrames; }
    QString status() const { return m_status; }
    bool hasError() const { return m_hasError; }
    QString saveFolder() const { return m_saveFolder; }
    QString properties() const { return m_properties; }
    QString camera2Report() const { return m_camera2Report; }
    int rotationIndex() const { return m_rotationIndex; }

    Q_INVOKABLE void chooseCamera(int index);
    Q_INVOKABLE void chooseSize(int index);
    Q_INVOKABLE void chooseFps(int index);
    Q_INVOKABLE void takePhoto();
    Q_INVOKABLE void toggleRecording();
    Q_INVOKABLE void setSaveFolder(const QString &path);
    Q_INVOKABLE void reconnect();
    Q_INVOKABLE void chooseRotation(int index);
    Q_INVOKABLE void refreshCamera2Report();
    Q_INVOKABLE QString filterText(const QString &text, const QString &query) const;

signals:
    void optionsChanged();
    void deliveredFpsChanged();
    void recordingChanged();
    void statusChanged();
    void saveFolderChanged();
    void propertiesChanged();
    void camera2ReportChanged();
    void rotationChanged();
    void frameReady(const QImage &frame);

private:
    QString nextPath(const QString &extension) const;
    CaptureThread m_capture;
    QMutex m_previewMutex;
    QImage m_latestPreview;
    bool m_previewQueued = false;
    QProcess m_reportProcess;
    QStringList m_cameras, m_sizes, m_rates;
    int m_cameraIndex = -1, m_sizeIndex = -1, m_fpsIndex = -1, m_targetFps = 0;
    double m_deliveredFps = 0;
    bool m_recording = false, m_hasError = false;
    int m_droppedFrames = 0;
    QString m_status = QStringLiteral("Connecting to camera…");
    QString m_saveFolder, m_properties, m_camera2Report;
    int m_rotationIndex = 0;
    bool m_captureStarted = false;
};
