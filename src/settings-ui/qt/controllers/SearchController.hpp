#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QStringList>
#include <vector>

namespace tinexus::settings_ui {

struct SettingSearchItem {
    QString title;
    QString subtitle;
    int pageIndex;
    QString iconId;
    QString iconColor;
    QStringList keywords;
};

class SearchController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY searchQueryChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QVariantList matchingPageIndices READ matchingPageIndices NOTIFY searchResultsChanged)

public:
    explicit SearchController(QObject* parent = nullptr);
    ~SearchController() override = default;

    QString searchQuery() const { return m_searchQuery; }
    bool isSearching() const { return !m_searchQuery.trimmed().isEmpty(); }
    QVariantList searchResults() const { return m_searchResults; }
    QVariantList matchingPageIndices() const { return m_matchingPageIndices; }

    Q_INVOKABLE bool isPageMatching(int pageIndex) const;

public slots:
    void setSearchQuery(const QString& query);
    void clearSearch();

signals:
    void searchQueryChanged();
    void searchResultsChanged();

private:
    void buildIndex();
    void performSearch();

    QString m_searchQuery;
    QVariantList m_searchResults;
    QVariantList m_matchingPageIndices;
    std::vector<SettingSearchItem> m_index;
};

} // namespace tinexus::settings_ui
