#pragma once

#include <QObject>
#include <QString>

namespace tinexus::settings_ui {

class AboutController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString osVersion READ osVersion NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString memInfo READ memInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString storageInfo READ storageInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString displayInfo READ displayInfo NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString graphicsEngine READ graphicsEngine NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString platformVersion READ platformVersion NOTIFY aboutInfoChanged)
    Q_PROPERTY(QString compositorInfo READ compositorInfo NOTIFY aboutInfoChanged)

    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY userInfoChanged)
    Q_PROPERTY(QString currentUserRealName READ currentUserRealName NOTIFY userInfoChanged)
    Q_PROPERTY(QString userInitials READ userInitials NOTIFY userInfoChanged)

public:
    explicit AboutController(QObject* parent = nullptr);
    ~AboutController() override = default;

    QString osVersion() const { return m_osVersion; }
    QString cpuModel() const { return m_cpuModel; }
    QString memInfo() const { return m_memInfo; }
    QString storageInfo() const { return m_storageInfo; }
    QString displayInfo() const { return m_displayInfo; }
    QString graphicsEngine() const { return m_graphicsEngine; }
    QString platformVersion() const { return m_platformVersion; }
    QString compositorInfo() const { return m_compositorInfo; }

    QString currentUserName() const { return m_currentUserName; }
    QString currentUserRealName() const { return m_currentUserRealName; }
    QString userInitials() const { return m_userInitials; }

public slots:
    void refreshSystemInfo();
    void refreshUserInfo();
    void checkForUpdates();

signals:
    void aboutInfoChanged();
    void userInfoChanged();
    void toastRequested(const QString& message, bool isError);

private:
    QString m_osVersion;
    QString m_cpuModel;
    QString m_memInfo;
    QString m_storageInfo;
    QString m_displayInfo;
    QString m_graphicsEngine;
    QString m_platformVersion;
    QString m_compositorInfo;

    QString m_currentUserName;
    QString m_currentUserRealName;
    QString m_userInitials;
};

} // namespace tinexus::settings_ui
