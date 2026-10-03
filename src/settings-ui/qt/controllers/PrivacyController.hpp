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

public:
    explicit PrivacyController(QObject* parent = nullptr);
    ~PrivacyController() override = default;

    QVariantList unverifiedApps() const { return m_unverifiedApps; }
    int unverifiedAppsCount() const { return static_cast<int>(m_unverifiedApps.size()); }

public slots:
    void rescanApps();
    void trustApp(const QString& hash);

signals:
    void unverifiedAppsChanged();
    void toastRequested(const QString& message, bool isError);

private:
    QVariantList m_unverifiedApps;
};

} // namespace tinexus::settings_ui
