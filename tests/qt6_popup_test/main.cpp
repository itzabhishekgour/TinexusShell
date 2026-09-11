#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("tinexus-qt6-popup-test");
    app.setOrganizationName("Tinexus");

    QQuickStyle::setStyle("Basic");

    QString qmlPath = qEnvironmentVariable("TINEXUS_POPUP_TEST_QML");
    if (qmlPath.isEmpty()) {
        QString binaryDir = QCoreApplication::applicationDirPath();
        QString candidate = binaryDir + "/../../tests/qt6_popup_test/main.qml";
        if (QFile::exists(candidate)) {
            qmlPath = QFileInfo(candidate).canonicalFilePath();
        } else {
            qmlPath = "/mnt/e/Tinu's Technology/Tinexus Manager/tests/qt6_popup_test/main.qml";
        }
    }

    QQmlApplicationEngine engine;
    const QUrl url = QUrl::fromLocalFile(qmlPath);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qCritical() << "Failed to load QML component from:" << objUrl;
                QCoreApplication::exit(-1);
            } else {
                qInfo() << "[Qt6-Popup-Test] QML UI loaded successfully onto Wayland display:"
                        << qgetenv("WAYLAND_DISPLAY") << "from:" << objUrl;
            }
        },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No root objects loaded from:" << qmlPath;
        return -1;
    }

    // Explicitly configure transientParent on any child Window
    QObject *rootObj = engine.rootObjects().value(0);
    QQuickWindow *rootWin = qobject_cast<QQuickWindow*>(rootObj);
    if (rootWin) {
        for (QQuickWindow *childWin : rootWin->findChildren<QQuickWindow*>()) {
            if (childWin && childWin != rootWin) {
                childWin->setTransientParent(rootWin);
                qInfo() << "[Qt6-Popup-Test] Connected transientParent for child window:"
                        << childWin->objectName() << "flags:" << childWin->flags();
            }
        }
    }

    return app.exec();
}
