#include "serverListCache.h"
#include "flatpakUtils.h"
#include "../debug.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <memory>
#include <utility>

namespace
{
// Read by a shell on the host, where the CLI keeps its cache: the Flatpak
// sandbox has its own XDG_CACHE_HOME and cannot see the host's ~/.cache, and
// this way no extra sandbox permission is needed.
constexpr const char* READ_CACHE_SCRIPT = R"(cat "${XDG_CACHE_HOME:-$HOME/.cache}/Proton/VPN/serverlist.json")";

// A read that finds the file cut short (see the header) is tried again after
// this wait, up to this many reads in all: some four seconds, where writing
// the file takes the CLI well under one.
constexpr int REREAD_DELAY_MS   = 500;
constexpr int MAX_READ_ATTEMPTS = 8;
} // namespace

ServerListCache& ServerListCache::instance()
{
    static ServerListCache cache;
    return cache;
}

ServerListCache::ServerListCache(QObject* parent)
    : QObject(parent)
{
}

void ServerListCache::reload()
{
    if (m_loading == true) return;
    m_loading = true;
    m_attempt = 1;
    readFile();
}

void ServerListCache::readFile()
{
    QProcess* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](const int exitCode, const QProcess::ExitStatus status)
    {
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0)
        {
            DBG_CLI(QStringLiteral("Could not read the CLI's server list cache: ")
                    + QString::fromUtf8(process->readAllStandardError()).trimmed());
            finish({});
            return;
        }
        parse(process->readAllStandardOutput());
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](const QProcess::ProcessError error)
    {
        // A process that ran reports through finished() instead.
        if (error != QProcess::FailedToStart) return;
        process->deleteLater();
        DBG_CLI(QStringLiteral("Could not start reading the CLI's server list cache."));
        finish({});
    });

    auto [program, args] = buildHostCommand(QStringLiteral("sh"),
                                            {QStringLiteral("-c"), QString::fromUtf8(READ_CACHE_SCRIPT)});
    process->start(program, args);
}

void ServerListCache::parse(const QByteArray& json)
{
    // Off the UI thread: parsing the whole file takes a noticeable moment.
    struct Result
    {
        QList<ServerList::Server> servers;
        QString error;
        qsizetype nodes = 0;
        qint64 elapsedMs = 0;
    };
    std::shared_ptr<Result> result = std::make_shared<Result>();
    QThread* thread = QThread::create([json, result]()
    {
        QElapsedTimer timer;
        timer.start();
        result->servers = ServerList::parse(json, &result->error);
        for (const ServerList::Server& server : result->servers)
        {
            result->nodes += server.nodes.size();
        }
        result->elapsedMs = timer.elapsed();
    });
    connect(thread, &QThread::finished, this, [this, thread, result]()
    {
        thread->deleteLater();
        if (result->error.isEmpty() == false)
        {
            if (m_attempt < MAX_READ_ATTEMPTS)
            {
                DBG_CLI(QStringLiteral("The CLI's server list cache was cut short (%1), likely being rewritten "
                                       "by the CLI; reading it again in %2 ms (attempt %3 of %4).")
                            .arg(result->error).arg(REREAD_DELAY_MS).arg(m_attempt + 1).arg(MAX_READ_ATTEMPTS));
                ++m_attempt;
                QTimer::singleShot(REREAD_DELAY_MS, this, &ServerListCache::readFile);
                return;
            }
            DBG_CLI(QStringLiteral("The CLI's server list cache was still not valid after %1 reads (%2).")
                        .arg(MAX_READ_ATTEMPTS).arg(result->error));
        }
        else if (result->servers.isEmpty() == true)
        {
            DBG_CLI(QStringLiteral("The CLI's server list cache lists no servers."));
        }
        else
        {
            DBG_CLI(QStringLiteral("Read %1 servers (%2 nodes) from the CLI's server list cache in %3 ms.")
                        .arg(result->servers.size()).arg(result->nodes).arg(result->elapsedMs));
        }
        finish(std::move(result->servers));
    });
    thread->start();
}

void ServerListCache::finish(QList<ServerList::Server> servers)
{
    m_loading = false;
    const bool ok = servers.isEmpty() == false;
    if (ok == true)
    {
        m_servers = std::move(servers);
        m_hasServers = true;
    }
    emit loaded(ok);
}
