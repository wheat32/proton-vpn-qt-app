#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <iterator>
#include "appConfig.h"
#include "connectionHistory.h"
#include "jsonFile.h"

namespace
{
QString historyFilePath()
{
    // XDG_DATA_HOME / ProtonVPN-Qt / history.json
    // Override to be explicit about the sub-dir name.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/ProtonVPN-Qt");
    return dir + QStringLiteral("/history.json");
}
} // namespace

ConnectionHistory& ConnectionHistory::instance()
{
    static ConnectionHistory inst;
    return inst;
}

ConnectionHistory::ConnectionHistory(QObject* parent)
    : QObject(parent)
{
    load();
}

QList<ConnectionEntry> ConnectionHistory::entries() const
{
    const int maxCount = AppConfig::instance().recentConnectionsCount();
    if (maxCount == 0)
    {
        return {};
    }
    if (m_entries.size() <= maxCount)
    {
        return m_entries;
    }
    return m_entries.sliced(0, maxCount);
}

void ConnectionHistory::record(const QString& countryCode,
                                const QString& countryName,
                                const QString& city)
{
    const int maxCount = AppConfig::instance().recentConnectionsCount();

    // 0 means the feature is disabled - don't store anything.
    if (maxCount == 0) return;

    // If already present, update timestamp and move to front
    const auto it = std::ranges::find_if(m_entries,
        [&countryCode, &city](const ConnectionEntry& e)
        {
            return e.countryCode == countryCode && e.city == city;
        });

    if (it != m_entries.end())
    {
        it->connectedAt = QDateTime::currentDateTime();
        it->countryName = countryName; // keep name fresh
        // Move to front
        const int idx = static_cast<int>(std::ranges::distance(m_entries.begin(), it));
        m_entries.move(idx, 0);
        save();
        emit changed();
        return;
    }

    // New entry - prepend
    ConnectionEntry e;
    e.countryCode  = countryCode;
    e.countryName  = countryName;
    e.city         = city;
    e.connectedAt  = QDateTime::currentDateTime();
    m_entries.prepend(e);

    // Trim to max
    // ReSharper disable once CppDFALoopConditionNotUpdated
    while (m_entries.size() > maxCount)
    {
        m_entries.removeLast();
    }

    save();
    emit changed();
}

void ConnectionHistory::clear()
{
    m_entries.clear();
    save();
    emit changed();
}

void ConnectionHistory::trimToCount(const int newMax)
{
    if (newMax <= 0)
    {
        if (m_entries.isEmpty() == false)
        {
            m_entries.clear();
            save();
            emit changed();
        }
        return;
    }

    if (m_entries.size() > newMax)
    {
        m_entries = m_entries.sliced(0, newMax);
        save();
        emit changed();
    }
}

void ConnectionHistory::load()
{
    QFile f(historyFilePath());
    if (f.open(QIODevice::ReadOnly) == false) return;

    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
    f.close();

    m_entries.clear();
    for (const QJsonValue& val : arr)
    {
        const QJsonObject obj = val.toObject();
        ConnectionEntry e;
        e.countryCode  = obj.value(QStringLiteral("country_code")).toString();
        e.countryName  = obj.value(QStringLiteral("country_name")).toString();
        e.city         = obj.value(QStringLiteral("city")).toString();
        e.connectedAt  = QDateTime::fromString(
            obj.value(QStringLiteral("connected_at")).toString(), Qt::ISODate);
        if (e.countryCode.isEmpty() == false)
        {
            m_entries.append(e);
        }
    }
}

void ConnectionHistory::save() const
{
    QJsonArray arr;
    for (const ConnectionEntry& e : m_entries)
    {
        QJsonObject obj;
        obj[QStringLiteral("country_code")]  = e.countryCode;
        obj[QStringLiteral("country_name")]  = e.countryName;
        obj[QStringLiteral("city")]          = e.city;
        obj[QStringLiteral("connected_at")]  = e.connectedAt.toString(Qt::ISODate);
        arr.append(obj);
    }

    (void)JsonFile::write(historyFilePath(), QJsonDocument(arr));
}

