#include "UsersController.hpp"
#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <fstream>
#include <sstream>
#include <pwd.h>
#include <unistd.h>
#include <thread>

namespace tinexus::settings_ui {

namespace {
bool isValidUnixUsername(const QString& u) {
    static const QRegularExpression re(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    return re.match(u).hasMatch();
}

bool userInSudoGroup(const std::string& username) {
    std::ifstream gf("/etc/group");
    if (!gf.is_open()) return false;
    std::string line;
    while (std::getline(gf, line)) {
        if (!line.starts_with("sudo:")) continue;
        auto lastColon = line.rfind(':');
        if (lastColon == std::string::npos) continue;
        std::stringstream ms(line.substr(lastColon + 1));
        std::string tok;
        while (std::getline(ms, tok, ',')) {
            if (tok == username) return true;
        }
    }
    return false;
}

QString initialsFromName(const QString& name) {
    QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        return (parts[0].left(1) + parts[1].left(1)).toUpper();
    }
    return name.left(2).toUpper();
}
} // namespace

UsersController::UsersController(QObject* parent)
    : QObject(parent)
{
    refreshUsers();
}

void UsersController::refreshUsers() {
    m_users.clear();
    if (struct passwd* cur = ::getpwuid(::getuid())) {
        m_currentUser = QString::fromLatin1(cur->pw_name);
    }

    std::ifstream pf("/etc/passwd");
    if (pf.is_open()) {
        std::string line;
        while (std::getline(pf, line)) {
            std::vector<std::string> fields;
            std::stringstream ss(line);
            std::string f;
            while (std::getline(ss, f, ':')) fields.push_back(f);
            if (fields.size() < 7) continue;
            int uid = -1;
            try { uid = std::stoi(fields[2]); } catch (...) { continue; }
            if (uid < 1000 || uid >= 60000) continue;

            const QString uname = QString::fromStdString(fields[0]);
            QString gecos = QString::fromStdString(fields[4]).section(QLatin1Char(','), 0, 0).trimmed();
            if (gecos.isEmpty()) gecos = uname;

            QVariantMap u;
            u[QStringLiteral("username")] = uname;
            u[QStringLiteral("realName")] = gecos;
            u[QStringLiteral("uid")] = uid;
            u[QStringLiteral("isAdmin")] = userInSudoGroup(fields[0]);
            u[QStringLiteral("isCurrent")] = (uname == m_currentUser);
            u[QStringLiteral("initials")] = initialsFromName(gecos);
            m_users.append(u);
        }
    }
    emit usersChanged();
}

void UsersController::createUser(const QString& username, const QString& fullName, const QString& password, bool isAdmin) {
    const QString uname = username.trimmed().toLower();
    if (!isValidUnixUsername(uname)) {
        emit toastRequested(QStringLiteral("Invalid username (use lowercase letters, digits, '-' or '_')"), true);
        return;
    }
    if (password.size() < 4) {
        emit toastRequested(QStringLiteral("Password must be at least 4 characters"), true);
        return;
    }

    const QString safeReal = fullName.trimmed().isEmpty() ? uname : fullName.trimmed();
    QPointer<UsersController> self(this);
    std::thread([self, uname, safeReal, password, isAdmin]() {
        QProcess add;
        QStringList args{QStringLiteral("/usr/sbin/useradd"), QStringLiteral("-m"),
                         QStringLiteral("-s"), QStringLiteral("/bin/bash"),
                         QStringLiteral("-c"), safeReal, uname};
        add.start(QStringLiteral("/usr/bin/sudo"), args);
        if (!add.waitForFinished(5000) || add.exitCode() != 0) {
            QString err = QString::fromUtf8(add.readAllStandardError()).trimmed();
            if (self) {
                QMetaObject::invokeMethod(self, [self, err]() {
                    if (self) emit self->toastRequested(err.isEmpty() ? QStringLiteral("Failed to create user") : err, true);
                }, Qt::QueuedConnection);
            }
            return;
        }

        QProcess chp;
        chp.start(QStringLiteral("/usr/bin/sudo"), {QStringLiteral("/usr/sbin/chpasswd")});
        chp.write((uname + QLatin1Char(':') + password + QLatin1Char('\n')).toUtf8());
        chp.closeWriteChannel();
        chp.waitForFinished(5000);

        if (isAdmin) {
            QProcess mod;
            mod.start(QStringLiteral("/usr/bin/sudo"),
                      {QStringLiteral("/usr/sbin/usermod"), QStringLiteral("-aG"), QStringLiteral("sudo"), uname});
            mod.waitForFinished(3000);
        }

        if (self) {
            QMetaObject::invokeMethod(self, [self, uname]() {
                if (!self) return;
                self->refreshUsers();
                emit self->toastRequested(QStringLiteral("Account '%1' created").arg(uname), false);
            }, Qt::QueuedConnection);
        }
    }).detach();
}

void UsersController::changePassword(const QString& username, const QString& newPassword) {
    const QString uname = username.trimmed();
    if (!isValidUnixUsername(uname) || newPassword.size() < 4) {
        emit toastRequested(QStringLiteral("Password must be at least 4 characters"), true);
        return;
    }

    QPointer<UsersController> self(this);
    std::thread([self, uname, newPassword]() {
        QProcess chp;
        chp.start(QStringLiteral("/usr/bin/sudo"), {QStringLiteral("/usr/sbin/chpasswd")});
        chp.write((uname + QLatin1Char(':') + newPassword + QLatin1Char('\n')).toUtf8());
        chp.closeWriteChannel();
        const bool ok = chp.waitForFinished(5000) && chp.exitCode() == 0;
        if (self) {
            QMetaObject::invokeMethod(self, [self, uname, ok]() {
                if (!self) return;
                emit self->toastRequested(ok ? QStringLiteral("Updated password for '%1'").arg(uname)
                                             : QStringLiteral("Failed to update password"), !ok);
            }, Qt::QueuedConnection);
        }
    }).detach();
}

void UsersController::deleteUser(const QString& username) {
    const QString uname = username.trimmed();
    if (!isValidUnixUsername(uname) || uname == m_currentUser) {
        emit toastRequested(QStringLiteral("Cannot delete the currently signed-in account"), true);
        return;
    }

    QPointer<UsersController> self(this);
    std::thread([self, uname]() {
        QProcess del;
        del.start(QStringLiteral("/usr/bin/sudo"),
                  {QStringLiteral("/usr/sbin/userdel"), QStringLiteral("-r"), uname});
        const bool ok = del.waitForFinished(6000) && del.exitCode() == 0;
        if (self) {
            QMetaObject::invokeMethod(self, [self, uname, ok]() {
                if (!self) return;
                self->refreshUsers();
                emit self->toastRequested(ok ? QStringLiteral("Removed account '%1'").arg(uname)
                                             : QStringLiteral("Failed to remove account '%1'").arg(uname), !ok);
            }, Qt::QueuedConnection);
        }
    }).detach();
}

} // namespace tinexus::settings_ui
