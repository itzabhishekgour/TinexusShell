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
#include <sys/socket.h>
#include <sys/un.h>
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
            // Existing primary instance is running -> signal it via UNIX domain socket and exit
            const char* rundir = getenv("XDG_RUNTIME_DIR");
            std::string sock_path = rundir ? (std::string(rundir) + "/tinexus-launcher-single") : "/tmp/tinexus-launcher-single";
            int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (s >= 0) {
                struct sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
                if (::connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                    const char* msg = "toggle\n";
                    ssize_t wres = ::write(s, msg, std::strlen(msg));
                    (void)wres;
                }
                ::close(s);
            }
            ::close(lock_fd);
            tinexus::log::info("[Launcher] Existing instance detected via flock; sent toggle request and exiting.");
            return 0;
        }
    }

    // Force Wayland platform and LayerShell integration
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland");
    }
    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");

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

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
    auto* lsWin = LayerShellQt::Window::get(window);
    if (lsWin) {
        lsWin->setLayer(LayerShellQt::Window::LayerTop);
        lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        lsWin->setScope("launcher");
        lsWin->setAnchors(LayerShellQt::Window::Anchors{});
        lsWin->setExclusiveZone(0);
        tinexus::log::info("[Launcher] LayerShellQt initialized: Layer=Top, KeyboardInteractivity=Exclusive, Scope=launcher");
    } else {
        tinexus::log::warn("[Launcher] LayerShellQt::Window::get returned nullptr; running in standard window mode.");
    }
#endif

    QObject::connect(&bridge, &tinexus::launcher::LauncherBridge::closeRequested, window, &QQuickWindow::hide);

    QLocalServer server;
    QLocalServer::removeServer(socketName);
    if (server.listen(socketName)) {
        std::string full_path = server.fullServerName().toStdString();
        ::chmod(full_path.c_str(), 0666);
        if (full_path != "/tmp/tinexus-launcher-single") {
            ::unlink("/tmp/tinexus-launcher-single");
            ::symlink(full_path.c_str(), "/tmp/tinexus-launcher-single");
        }
        tinexus::log::info("[Launcher] Listening on socket '{}' (mode 0666)", full_path);

        QObject::connect(&server, &QLocalServer::newConnection, [&server, window]() {
            while (auto* client = server.nextPendingConnection()) {
                auto processMessage = [client, window]() {
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
                };

                if (client->bytesAvailable() > 0 || client->state() == QLocalSocket::UnconnectedState) {
                    processMessage();
                } else {
                    QObject::connect(client, &QLocalSocket::readyRead, processMessage);
                    QObject::connect(client, &QLocalSocket::disconnected, processMessage);
                }
            }
        });
    }

    bool isDaemon = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--daemon" || std::string_view(argv[i]) == "--preheat") {
            isDaemon = true;
            break;
        }
    }
    if (!isDaemon) {
        window->show();
        window->raise();
        window->requestActivate();
    }
    return app.exec();
}
