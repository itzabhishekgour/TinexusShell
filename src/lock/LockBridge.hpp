// ============================================================================
// LockBridge.hpp — Qt6 Bridge for tinexus-lock with Real PAM Authentication
// ============================================================================
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>

namespace tinexus::lock {

class LockBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentTime READ currentTime NOTIFY timeChanged)
    Q_PROPERTY(QString currentDate READ currentDate NOTIFY timeChanged)
    Q_PROPERTY(QString username READ username CONSTANT)
    Q_PROPERTY(bool isAuthenticating READ isAuthenticating NOTIFY authStateChanged)
    Q_PROPERTY(bool authFailed READ authFailed NOTIFY authFailedChanged)

public:
    explicit LockBridge(QObject* parent = nullptr);
    ~LockBridge() override = default;

    QString currentTime() const { return m_currentTime; }
    QString currentDate() const { return m_currentDate; }
    QString username() const { return m_username; }
    bool isAuthenticating() const { return m_isAuthenticating; }
    bool authFailed() const { return m_authFailed; }

    Q_INVOKABLE void authenticate(const QString& password);
    Q_INVOKABLE void resetAuthFailed();

signals:
    void timeChanged();
    void authStateChanged();
    void authFailedChanged();
    void unlockSuccess();

private slots:
    void updateClock();

private:
    bool verifyPam(const std::string& password);

    QString m_currentTime;
    QString m_currentDate;
    QString m_username;
    bool    m_isAuthenticating{false};
    bool    m_authFailed{false};
    QTimer  m_clockTimer;
};

} // namespace tinexus::lock
