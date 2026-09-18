#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>

namespace tinexus::settings_ui {

class PersonalizationController : public QObject {
    Q_OBJECT

    Q_PROPERTY(int accentIndex READ accentIndex WRITE setAccentIndex NOTIFY accentIndexChanged)
    Q_PROPERTY(QString accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(int selectedWallpaperIndex READ selectedWallpaperIndex WRITE setSelectedWallpaperIndex NOTIFY selectedWallpaperIndexChanged)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY wallpapersChanged)

public:
    explicit PersonalizationController(QObject* parent = nullptr);
    ~PersonalizationController() override = default;

    int accentIndex() const { return m_accentIndex; }
    QString accentColor() const;
    QString themeMode() const { return m_themeMode; }
    int selectedWallpaperIndex() const { return m_selectedWallpaperIndex; }
    QVariantList wallpapers() const { return m_wallpapers; }

public slots:
    void setAccentIndex(int index);
    void setThemeMode(const QString& mode);
    void setSelectedWallpaperIndex(int index);
    void scanWallpapers();

signals:
    void accentIndexChanged();
    void accentColorChanged();
    void themeModeChanged();
    void selectedWallpaperIndexChanged();
    void wallpapersChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    int m_accentIndex{0};
    QString m_themeMode{"Dark"};
    int m_selectedWallpaperIndex{0};
    QVariantList m_wallpapers;
};

} // namespace tinexus::settings_ui
