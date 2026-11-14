#include "CameraEngine.h"
#include "PreviewItem.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Camera2Termux Studio"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    qmlRegisterType<PreviewItem>("Camera2Termux.Studio", 1, 0, "CameraPreview");
    CameraEngine studio;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("studio"), &studio);
    QString qml = QCoreApplication::applicationDirPath() +
                  QStringLiteral("/../share/camera2termux/studio/Main.qml");
    if (!QFileInfo::exists(qml)) qml = QStringLiteral(STUDIO_SOURCE_DIR "/Main.qml");
    engine.load(QUrl::fromLocalFile(qml));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *preview = engine.rootObjects().first()->findChild<PreviewItem *>(QStringLiteral("previewItem"));
    if (!preview) return 1;
    QObject::connect(&studio, &CameraEngine::frameReady, preview, &PreviewItem::setFrame);
    return app.exec();
}
