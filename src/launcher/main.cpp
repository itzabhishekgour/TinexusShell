// ============================================================================
// main.cpp — tinexus-launcher (Qt6)
// ============================================================================
#include "LauncherBridge.hpp"
#include <common/logger.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QEvent>
#include <QtNetwork/QLocalSocket>
#include <QtNetwork/QLocalServer>
#include <common/RuntimePaths.hpp>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <iostream>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

namespace {
class WindowCloseFilter : public QObject {
public:
    explicit WindowCloseFilter(QQuickWindow* w) : m_win(w) {}
protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::Close) {
            ev->ignore();
            if (m_win) m_win->hide();
            return true;
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    QQuickWindow* m_win{nullptr};
};
} // namespace

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("launcher");
    tinexus::log::info("tinexus-launcher starting (Qt6)...");

    const QString socketName = QStringLiteral("tinexus-launcher-single");

    tinexus::common::RuntimePaths::ensure_runtime_dir();
    std::string lock_path = tinexus::common::RuntimePaths::get_app_lock_path("tinexus-launcher");
    int lock_fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock_fd >= 0) {
        int res = ::flock(lock_fd, LOCK_EX | LOCK_NB);
        if (res != 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            // Existing primary instance is running -> signal it and exit
            QGuiApplication tempApp(argc, argv);
            QLocalSocket probeSocket;
            probeSocket.connectToServer(socketName);
            if (probeSocket.waitForConnected(500)) {
                probeSocket.write("toggle\n");
                probeSocket.waitForBytesWritten(500);
                probeSocket.flush();
            }
            ::close(lock_fd);
            tinexus::log::info("[Launcher] Existing instance detected via flock; sent toggle request and exiting.");
            return 0;
        }
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-launcher"));
    app.setDesktopFileName(QStringLiteral("tinexus-launcher"));

    tinexus::launcher::LauncherBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/LauncherWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/launcher/qml/LauncherWindow.qml"),
        QStringLiteral("src/launcher/qml/LauncherWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/launcher/qml/LauncherWindow.qml"),
        QStringLiteral("/usr/share/tinexus/launcher/qml/LauncherWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate LauncherWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for launcher" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        return 1;
    }

    auto* closeFilter = new WindowCloseFilter(window);
    window->installEventFilter(closeFilter);

    QObject::connect(&bridge, &tinexus::launcher::LauncherBridge::closeRequested, window, &QQuickWindow::hide);

    QLocalServer server;
    QLocalServer::removeServer(socketName);
    if (server.listen(socketName)) {
        QObject::connect(&server, &QLocalServer::newConnection, [&server, window]() {
            while (auto* client = server.nextPendingConnection()) {
                QObject::connect(client, &QLocalSocket::readyRead, [client, window]() {
                    QByteArray data = client->readAll();
                    if (data.contains("toggle")) {
                        if (window) {
                            if (window->isVisible()) {
                                window->hide();
                            } else {
                                window->show();
                                window->raise();
                                window->requestActivate();
                            }
                        }
                    }
                    client->disconnectFromServer();
                });
            }
        });
    }

    window->show();
    return app.exec();
}
