// ============================================================================
// LauncherBridge.cpp — C++20 QObject Bridge for tinexus-launcher
// ============================================================================
#include "LauncherBridge.hpp"
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QRegularExpression>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <common/DBusNames.hpp>
#include <sstream>
#include <algorithm>

namespace tinexus::launcher {

LauncherBridge::LauncherBridge(QObject* parent)
    : QObject(parent)
{
    m_allApps = {
        LauncherItem{"Tinexus Terminal", "tinexus-terminal", "Default Wayland GPU Terminal", "utilities-terminal", "App", true},
        LauncherItem{"Firefox", "env MOZ_ENABLE_WAYLAND=1 firefox", "Mozilla Firefox Web Browser", "firefox", "App", false},
        LauncherItem{"Tinexus Files", "tinexus-files", "Lightweight Miller Column File Manager", "system-file-manager", "App", false},
        LauncherItem{"Tinexus Settings", "tinexus-settings-ui", "System Configuration & Control Center", "preferences-desktop", "App", false},
        LauncherItem{"Activity Monitor", "tinexus-monitor", "Platform Resource & Process Monitor", "utilities-system-monitor", "App", false},
        LauncherItem{"Tinexus Package Manager", "tinexus-pkg", "Package Installer & Software Manager", "system-software-install", "App", false},
        LauncherItem{"Foot Terminal", "foot", "Wayland Lightweight Terminal Emulator", "utilities-terminal", "App", false},

        // Restored System Actions (from txui LauncherWidget.cpp §get_system_actions)
        LauncherItem{"Lock Screen", "tinexus-lock", "Lock the current desktop session", "system-lock-screen", "System", false},
        LauncherItem{"Restart", "systemctl reboot", "Reboot the platform", "system-reboot", "System", false},
        LauncherItem{"Shut Down", "systemctl poweroff", "Safely power off the computer", "system-shutdown", "System", false},
        LauncherItem{"Sleep", "systemctl suspend", "Suspend system to RAM", "system-suspend", "System", false},
        LauncherItem{"Log Out", "tinexus-session logout", "End current user session", "system-log-out", "System", false}
    };

    scanDesktopApps();
    refreshResults();
}

void LauncherBridge::scanDesktopApps() {
    const QStringList appDirs = {
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/usr/local/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/applications")
    };

    for (const auto& dirPath : appDirs) {
        QDir appsDir(dirPath);
        if (!appsDir.exists()) continue;

        const auto entries = appsDir.entryInfoList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const auto& info : entries) {
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

            QString name, exec, icon, comment;
            bool noDisplay = false;
            bool isDesktopEntry = false;
            bool terminal = false;

            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line == QStringLiteral("[Desktop Entry]")) {
                    isDesktopEntry = true;
                    continue;
                }
                if (line.startsWith(QLatin1Char('['))) {
                    isDesktopEntry = false;
                }
                if (!isDesktopEntry) continue;

                if (line.startsWith(QStringLiteral("Name=")) && name.isEmpty()) {
                    name = line.mid(5).trimmed();
                } else if (line.startsWith(QStringLiteral("Exec=")) && exec.isEmpty()) {
                    exec = line.mid(5).trimmed();
                    exec.remove(QRegularExpression(QStringLiteral("%[a-zA-Z]")));
                    exec = exec.trimmed();
                } else if (line.startsWith(QStringLiteral("Icon=")) && icon.isEmpty()) {
                    icon = line.mid(5).trimmed();
                } else if (line.startsWith(QStringLiteral("Comment=")) && comment.isEmpty()) {
                    comment = line.mid(8).trimmed();
                } else if (line.startsWith(QStringLiteral("Terminal=true"))) {
                    terminal = true;
                } else if (line.startsWith(QStringLiteral("NoDisplay=true"))) {
                    noDisplay = true;
                }
            }

            if (!noDisplay && !name.isEmpty() && !exec.isEmpty()) {
                // Check if already in list
                bool duplicate = false;
                for (const auto& app : m_allApps) {
                    if (app.name.compare(name, Qt::CaseInsensitive) == 0 ||
                        app.exec.compare(exec, Qt::CaseInsensitive) == 0) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    m_allApps.push_back(LauncherItem{
                        name,
                        exec,
                        comment.isEmpty() ? QStringLiteral("Application") : comment,
                        icon.isEmpty() ? QStringLiteral("application-x-executable") : icon,
                        QStringLiteral("App"),
                        terminal
                    });
                }
            }
        }
    }
}

void LauncherBridge::setAllApps(const std::vector<LauncherItem>& apps) {
    m_allApps = apps;
    refreshResults();
}

void LauncherBridge::setReducedMotion(bool rm) {
    if (m_reducedMotion != rm) {
        m_reducedMotion = rm;
        emit reducedMotionChanged();
    }
}

void LauncherBridge::setQuery(const QString& q) {
    if (m_query != q) {
        m_query = q;
        emit queryChanged();
        refreshResults();
        setSelectedIndex(0);
    }
}

void LauncherBridge::setSelectedIndex(int idx) {
    if (m_results.empty()) {
        m_selectedIndex = 0;
    } else {
        m_selectedIndex = std::clamp(idx, 0, static_cast<int>(m_results.size()) - 1);
    }
    emit selectedIndexChanged();
}

void LauncherBridge::selectNext() {
    if (m_results.empty()) return;
    setSelectedIndex((m_selectedIndex + 1) % static_cast<int>(m_results.size()));
}

void LauncherBridge::selectPrev() {
    if (m_results.empty()) return;
    setSelectedIndex((m_selectedIndex - 1 + static_cast<int>(m_results.size())) % static_cast<int>(m_results.size()));
}

void LauncherBridge::tabComplete() {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_results.size())) {
        setQuery(m_results[static_cast<size_t>(m_selectedIndex)].name);
    }
}

void LauncherBridge::launchSelected() {
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_results.size())) {
        launchIndex(m_selectedIndex);
    }
}

void LauncherBridge::launchIndex(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_results.size())) return;
    const auto& item = m_results[static_cast<size_t>(idx)];

    if (item.kind == QStringLiteral("Calculator")) {
        // Calculator copies result or closes
        closeLauncher();
        return;
    }

    spawnApp(item.exec, item.isTerminal);
    closeLauncher();
}

void LauncherBridge::pinToDock(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_results.size())) return;
    const auto& item = m_results[static_cast<size_t>(idx)];
    if (item.kind == QStringLiteral("Calculator") || item.kind == QStringLiteral("Store")) return;

    QString appId = item.exec;
    if (appId.startsWith(QStringLiteral("env "))) {
        QStringList p = appId.split(QLatin1Char(' '));
        appId = p.last();
    } else {
        QStringList p = appId.split(QLatin1Char(' '));
        if (!p.isEmpty()) appId = p.first();
    }

    pinToDockByAppId(appId);
}

void LauncherBridge::pinToDockByAppId(const QString& appId) {
    if (appId.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        tinexus::common::dbus::qservice::Dock(),
        tinexus::common::dbus::qpath::Dock(),
        tinexus::common::dbus::qinterface::Dock(),
        QStringLiteral("PinApp")
    );
    msg << appId;
    QDBusConnection::sessionBus().send(msg);
}

static void enrichEnvironmentWithRuntime(QProcessEnvironment& env) {
    env.remove(QStringLiteral("QT_WAYLAND_SHELL_INTEGRATION"));
    if (!env.contains(QStringLiteral("DISPLAY")) || !env.contains(QStringLiteral("WAYLAND_DISPLAY"))) {
        QFile envFile(QStringLiteral("/run/tinexus/env"));
        if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!envFile.atEnd()) {
                QByteArray line = envFile.readLine().trimmed();
                int eq = line.indexOf('=');
                if (eq > 0) {
                    QString k = QString::fromUtf8(line.left(eq));
                    QString v = QString::fromUtf8(line.mid(eq + 1));
                    if (!env.contains(k)) {
                        env.insert(k, v);
                    }
                }
            }
        }
    }
}

void LauncherBridge::closeLauncher() {
    setQuery(QString());
    emit closeRequested();
}

void LauncherBridge::spawnApp(const QString& execCmd, bool inTerminal) {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    enrichEnvironmentWithRuntime(env);

    if (inTerminal) {
        QProcess process;
        process.setProgram(QStringLiteral("tinexus-terminal"));
        process.setArguments({QStringLiteral("-e"), execCmd});
        process.setProcessEnvironment(env);
        process.startDetached();
    } else {
        QStringList args = QProcess::splitCommand(execCmd);
        if (!args.isEmpty()) {
            QString prog = args.takeFirst();
            QProcess process;
            process.setProgram(prog);
            process.setArguments(args);
            process.setProcessEnvironment(env);
            process.startDetached();
        }
    }
}

bool LauncherBridge::tryEvalCalc(const QString& q, double& outVal) {
    std::string s = q.trimmed().toStdString();
    if (s.empty()) return false;

    // Check for arithmetic operator: +, -, *, /
    size_t op_pos = std::string::npos;
    char op = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (i > 0 && i < s.size() - 1) {
                op_pos = i;
                op = c;
                break;
            }
        }
    }
    if (op_pos == std::string::npos) return false;

    std::string lhs = s.substr(0, op_pos);
    std::string rhs = s.substr(op_pos + 1);

    try {
        size_t idx1 = 0, idx2 = 0;
        double a = std::stod(lhs, &idx1);
        double b = std::stod(rhs, &idx2);
        if (op == '+') outVal = a + b;
        else if (op == '-') outVal = a - b;
        else if (op == '*') outVal = a * b;
        else if (op == '/') {
            if (b == 0.0) return false;
            outVal = a / b;
        }
        return true;
    } catch (...) {
        return false;
    }
}

void LauncherBridge::refreshResults() {
    m_results.clear();
    const QString trimmed = m_query.trimmed();

    // 1. Calculator Check
    double calcResult = 0.0;
    if (!trimmed.isEmpty() && tryEvalCalc(trimmed, calcResult)) {
        LauncherItem calcItem;
        // Format nicely (e.g. 336 instead of 336.0000)
        std::ostringstream oss;
        if (std::floor(calcResult) == calcResult) {
            oss << static_cast<int64_t>(calcResult);
        } else {
            oss << calcResult;
        }
        calcItem.name = QString::fromStdString(oss.str());
        calcItem.exec = calcItem.name;
        calcItem.description = QStringLiteral("Calculation: ") + trimmed;
        calcItem.icon = QStringLiteral("accessories-calculator");
        calcItem.kind = QStringLiteral("Calculator");
        calcItem.isTerminal = false;
        m_results.push_back(calcItem);
    }

    // 2. Filter installed apps
    if (trimmed.isEmpty()) {
        m_results = m_allApps;
    } else {
        const QString lowerQ = trimmed.toLower();
        for (const auto& app : m_allApps) {
            if (app.name.toLower().contains(lowerQ) ||
                app.exec.toLower().contains(lowerQ) ||
                app.description.toLower().contains(lowerQ)) {
                m_results.push_back(app);
            }
        }
    }

    // 3. App Store Fallback Search
    if (m_results.empty() && !trimmed.isEmpty()) {
        LauncherItem storeItem;
        storeItem.name = QStringLiteral("Search App Store for \"") + trimmed + QStringLiteral("\"");
        storeItem.exec = QStringLiteral("tinexus-store");
        storeItem.description = QStringLiteral("Find and install \"") + trimmed + QStringLiteral("\" from the package repository");
        storeItem.icon = QStringLiteral("system-software-install");
        storeItem.kind = QStringLiteral("Store");
        storeItem.isTerminal = false;
        m_results.push_back(storeItem);
    }

    emit resultsChanged();
}

QVariantList LauncherBridge::resultsList() const {
    QVariantList list;
    list.reserve(static_cast<int>(m_results.size()));
    for (const auto& item : m_results) {
        QVariantMap map;
        map[QStringLiteral("name")]        = item.name;
        map[QStringLiteral("exec")]        = item.exec;
        map[QStringLiteral("description")] = item.description;
        map[QStringLiteral("icon")]        = item.icon;
        map[QStringLiteral("kind")]        = item.kind;
        map[QStringLiteral("isTerminal")]  = item.isTerminal;
        list.append(map);
    }
    return list;
}

} // namespace tinexus::launcher

#include "moc_LauncherBridge.cpp"
