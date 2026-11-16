#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include "favoritesManager.h"
#include "jsonFile.h"

namespace
{
QString favoritesFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/ProtonVPN-Qt");
    return dir + QStringLiteral("/favorites.json");
}
} // namespace

FavoritesManager& FavoritesManager::instance()
{
    static FavoritesManager inst;
    return inst;
}

FavoritesManager::FavoritesManager(QObject* parent)
    : QObject(parent)
{
    load();
}

QList<FavoriteEntry> FavoritesManager::entries() const
{
    return m_entries;
}

bool FavoritesManager::isFavorite(const QString& countryCode, const QString& city) const
{
    return std::ranges::any_of(m_entries, [&](const FavoriteEntry& e)
    {
        return e.countryCode.compare(countryCode, Qt::CaseInsensitive) == 0
            && e.city.compare(city, Qt::CaseInsensitive) == 0;
    });
}

void FavoritesManager::add(const QString& countryCode,
                            const QString& countryName,
                            const QString& city)
{
    if (isFavorite(countryCode, city) == true) return;

    FavoriteEntry e;
    e.countryCode = countryCode;
    e.countryName = countryName;
    e.city        = city;
    m_entries.append(e);
    save();
    emit changed();
}

void FavoritesManager::remove(const QString& countryCode, const QString& city)
{
    const auto it = std::ranges::find_if(m_entries, [&](const FavoriteEntry& e)
    {
        return e.countryCode.compare(countryCode, Qt::CaseInsensitive) == 0
            && e.city.compare(city, Qt::CaseInsensitive) == 0;
    });

    if (it == m_entries.end()) return;

    m_entries.erase(it);
    save();
    emit changed();
}

void FavoritesManager::toggle(const QString& countryCode,
                               const QString& countryName,
                               const QString& city)
{
    if (isFavorite(countryCode, city) == true)
    {
        remove(countryCode, city);
    }
    else
    {
        add(countryCode, countryName, city);
    }
}

void FavoritesManager::clear()
{
    if (m_entries.isEmpty() == true) return;
    m_entries.clear();
    save();
    emit changed();
}

void FavoritesManager::load()
{
    QFile f(favoritesFilePath());
    if (f.open(QIODevice::ReadOnly) == false) return;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    f.close();

    m_entries.clear();
    for (const QJsonValue& val : arr)
    {
        const QJsonObject obj = val.toObject();
        FavoriteEntry e;
        e.countryCode = obj.value(QStringLiteral("country_code")).toString();
        e.countryName = obj.value(QStringLiteral("country_name")).toString();
        e.city        = obj.value(QStringLiteral("city")).toString();
        if (e.countryCode.isEmpty() == false)
        {
            m_entries.append(e);
        }
    }
}

void FavoritesManager::save() const
{
    QJsonArray arr;
    for (const FavoriteEntry& e : m_entries)
    {
        QJsonObject obj;
        obj[QStringLiteral("country_code")] = e.countryCode;
        obj[QStringLiteral("country_name")] = e.countryName;
        obj[QStringLiteral("city")]         = e.city;
        arr.append(obj);
    }

    (void)JsonFile::write(favoritesFilePath(), QJsonDocument(arr));
}

