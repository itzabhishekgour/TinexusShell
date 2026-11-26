#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace tinexus::settings_ui {

class UsersController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList users READ users NOTIFY usersChanged)
    Q_PROPERTY(QString currentUser READ currentUser NOTIFY usersChanged)

public:
    explicit UsersController(QObject* parent = nullptr);
    ~UsersController() override = default;

    QVariantList users() const { return m_users; }
    QString currentUser() const { return m_currentUser; }

public slots:
    void refreshUsers();
    void createUser(const QString& username, const QString& fullName, const QString& password, bool isAdmin);
    void changePassword(const QString& username, const QString& newPassword);
    void deleteUser(const QString& username);

signals:
    void usersChanged();
    void toastRequested(const QString& message, bool isError);

private:
    QVariantList m_users;
    QString m_currentUser;
};

} // namespace tinexus::settings_ui
