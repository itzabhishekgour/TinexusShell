// ============================================================================
// main.cpp — tinexus-lock (Qt6 / Layer-shell / PAM)
// ============================================================================
#include "LockBridge.hpp"
#include <common/logger.hpp>
#include <common/RuntimePaths.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
#include <LayerShellQt/Window>
#endif

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("lock");
    tinexus::log::info("tinexus-lock starting (Qt6)...");

    // Single-instance guard
    tinexus::common::RuntimePaths::ensure_runtime_dir();
    std::string lock_path = tinexus::common::RuntimePaths::get_app_lock_path("tinexus-lock");
    int lock_fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock_fd >= 0) {
        int res = ::flock(lock_fd, LOCK_EX | LOCK_NB);
        if (res != 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            tinexus::log::warn("[lock] tinexus-lock is already active; exiting secondary instance.");
            ::close(lock_fd);
            return 0;
        }
    }

    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
    QGuiApplication app(argc, argv);
    qunsetenv("QT_WAYLAND_SHELL_INTEGRATION");
    app.setApplicationName(QStringLiteral("tinexus-lock"));
    app.setDesktopFileName(QStringLiteral("tinexus-lock"));

    tinexus::lock::LockBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/LockWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/lock/qml/LockWindow.qml"),
        QStringLiteral("src/lock/qml/LockWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/lock/qml/LockWindow.qml"),
        QStringLiteral("/usr/share/tinexus/lock/qml/LockWindow.qml"),
        QStringLiteral("/usr/share/tinexus-lock/qml/LockWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate LockWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for lockscreen" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
#if defined(HAVE_LAYERSHELL) && HAVE_LAYERSHELL
        auto* lsWin = LayerShellQt::Window::get(window);
        if (lsWin) {
            lsWin->setLayer(LayerShellQt::Window::LayerOverlay);
            lsWin->setAnchors(LayerShellQt::Window::Anchors::fromInt(LayerShellQt::Window::AnchorTop |
                                                                    LayerShellQt::Window::AnchorBottom |
                                                                    LayerShellQt::Window::AnchorLeft |
                                                                    LayerShellQt::Window::AnchorRight));
            lsWin->setExclusiveZone(-1);
            lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
            std::cout << "[tinexus-lock] LayerShellQt configured: Layer=Overlay, Fullscreen, Exclusive Keyboard" << std::endl;
        }
#endif
        window->show();
        window->requestActivate();
        window->raise();
    }

    QObject::connect(&bridge, &tinexus::lock::LockBridge::unlockSuccess, [&app]() {
        tinexus::log::info("[lock] Authentication successful — exiting lock screen cleanly.");
        app.quit();
    });

    return app.exec();
}
