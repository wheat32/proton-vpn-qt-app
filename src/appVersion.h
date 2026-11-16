#pragma once

// appVersion.h
// The contents of version.json, the single source of truth for the app's
// version and the CLI versions it was tested with. Every reader goes through
// here instead of parsing the embedded file itself.

#include <QByteArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace AppVersion
{
struct Info
{
    QString appVersion;   // "1.13.1"; empty when the file could not be read
    QString cliTestedMin; // oldest CLI the app was tested with
    QString cliTestedMax; // newest CLI the app was tested with
    bool    prerelease = false;
};

// Reads the fields out of version.json's contents. A missing key leaves its
// field empty (false for `prerelease`).
inline Info parse(const QByteArray& json)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    Info info;
    info.appVersion   = root.value(QStringLiteral("app_version")).toString();
    info.cliTestedMin = root.value(QStringLiteral("cli_version_tested_min")).toString();
    info.cliTestedMax = root.value(QStringLiteral("cli_version_tested_max")).toString();
    info.prerelease   = root.value(QStringLiteral("prerelease")).toBool(false);
    return info;
}

// The embedded :/version.json, read once.
inline const Info& embedded()
{
    static const Info info = []()
    {
        QFile file(QStringLiteral(":/version.json"));
        if (file.open(QIODevice::ReadOnly) == false)
        {
            return Info();
        }
        return parse(file.readAll());
    }();
    return info;
}
} // namespace AppVersion
