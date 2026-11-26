#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>

namespace tinexus::settings_ui {

class PrivacyController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList unverifiedApps READ unverifiedApps NOTIFY unverifiedAppsChanged)
    Q_PROPERTY(int unverifiedAppsCount READ unverifiedAppsCount NOTIFY unverifiedAppsChanged)
    Q_PROPERTY(bool guardEnabled READ guardEnabled WRITE setGuardEnabled NOTIFY securityPolicyChanged)
    Q_PROPERTY(bool sandboxEnabled READ sandboxEnabled WRITE setSandboxEnabled NOTIFY securityPolicyChanged)

public:
    explicit PrivacyController(QObject* parent = nullptr);
    ~PrivacyController() override = default;

    QVariantList unverifiedApps() const { return m_unverifiedApps; }
    int unverifiedAppsCount() const { return static_cast<int>(m_unverifiedApps.size()); }
    bool guardEnabled() const { return m_guardEnabled; }
    bool sandboxEnabled() const { return m_sandboxEnabled; }

public slots:
    void rescanApps();
    void trustApp(const QString& hash);
    void setGuardEnabled(bool enabled);
    void setSandboxEnabled(bool enabled);

signals:
    void unverifiedAppsChanged();
    void securityPolicyChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    QVariantList m_unverifiedApps;
    bool m_guardEnabled{true};
    bool m_sandboxEnabled{true};
};

} // namespace tinexus::settings_ui
