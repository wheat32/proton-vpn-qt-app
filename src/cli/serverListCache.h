#pragma once

#include <QList>
#include <QObject>
#include "serverList.h"

// The servers in the protonvpn CLI's server list cache (see serverList.h),
// read on request in the background: the file runs to tens of megabytes. The
// CLI keeps the file current itself, so each reload() reads it afresh.
//
// The CLI rewrites the file in place every quarter of an hour or so, to
// update the servers' loads: it empties the file and writes it out again,
// which takes it a moment. A read in that moment finds the JSON cut short,
// and is tried again after a short wait, a few times over.
class ServerListCache : public QObject
{
    Q_OBJECT

public:
    static ServerListCache& instance();

    // Reads the file again and emits loaded() once done. A call while a read
    // is under way waits for that read.
    void reload();
    // True once a read has succeeded; servers() then holds its result.
    [[nodiscard]] bool hasServers() const { return m_hasServers; }
    [[nodiscard]] const QList<ServerList::Server>& servers() const { return m_servers; }

signals:
    // `ok` is false when the file could not be read or held no servers;
    // servers() then still holds the last good read, if any.
    void loaded(bool ok);

private:
    explicit ServerListCache(QObject* parent = nullptr);
    void readFile();
    void parse(const QByteArray& json);
    // Ends a reload; `servers` is what it read, empty when it failed.
    void finish(QList<ServerList::Server> servers);

    QList<ServerList::Server> m_servers;
    bool m_hasServers = false;
    bool m_loading = false;
    int m_attempt = 0; // of the reload under way, from 1
};
