#include <QtTest/QtTest>
#include "cli/statusSnapshot.h"

// StatusSnapshot::apply() decides what a `protonvpn status` poll does to the
// connection state. Each rule here was a bug once: a failed connect's error
// wiped by the next poll, a connected server that was never reported after a
// connect made in the app, and a server still reported after disconnecting.

class TstStatusSnapshot : public QObject
{
    Q_OBJECT

private:
    static QMap<QString, QString> connectedTo(const QString& server)
    {
        return {{QStringLiteral("status"), QStringLiteral("Connected")},
                {QStringLiteral("server"), server}};
    }

    static QMap<QString, QString> disconnected()
    {
        return {{QStringLiteral("status"), QStringLiteral("Disconnected")}};
    }

    static QString nj() { return QStringLiteral("US-NJ#203 in Secaucus, United States"); }
    static QString ch() { return QStringLiteral("CH#12 in Zurich, Switzerland"); }

private slots:
    void apply_noStatusField_changesNothing()
    {
        const QMap<QString, QString> fields{{QStringLiteral("traceback"), QStringLiteral("boom")}};
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Connected, nj(), fields);

        QCOMPARE(outcome.readable, false);
        QCOMPARE(outcome.state, VpnState::Connected);
        QCOMPARE(outcome.server, nj());
        QCOMPARE(outcome.stateChanged, false);
    }

    void apply_unknownThenDisconnected_announcesDisconnected()
    {
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Unknown, QString(), disconnected());

        QCOMPARE(outcome.readable, true);
        QCOMPARE(outcome.state, VpnState::Disconnected);
        QCOMPARE(outcome.stateChanged, true);
        QVERIFY(outcome.server.isEmpty());
    }

    void apply_disconnectedThenConnected_announcesStateWithServer()
    {
        const StatusSnapshot::Outcome outcome =
            StatusSnapshot::apply(VpnState::Disconnected, QString(), connectedTo(nj()));

        QCOMPARE(outcome.state, VpnState::Connected);
        QCOMPARE(outcome.server, nj());
        QCOMPARE(outcome.stateChanged, true);
        QCOMPARE(outcome.serverLearned, false);
        QCOMPARE(outcome.serverChanged, false);
    }

    void apply_connectedThenDisconnected_announcesAndForgetsServer()
    {
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Connected, nj(), disconnected());

        QCOMPARE(outcome.state, VpnState::Disconnected);
        QCOMPARE(outcome.stateChanged, true);
        QVERIFY(outcome.server.isEmpty());
    }

    void apply_statusIsCaseInsensitive()
    {
        const QMap<QString, QString> fields{{QStringLiteral("status"), QStringLiteral("CONNECTED")},
                                            {QStringLiteral("server"), nj()}};
        QCOMPARE(StatusSnapshot::apply(VpnState::Disconnected, QString(), fields).state, VpnState::Connected);
    }

    //  Error stays on screen

    void apply_errorThenDisconnected_keepsError()
    {
        // A failed connect shows what went wrong. "Disconnected" from the next
        // poll is no news, and must not replace the message.
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Error, QString(), disconnected());

        QCOMPARE(outcome.readable, true);
        QCOMPARE(outcome.state, VpnState::Error);
        QCOMPARE(outcome.stateChanged, false);
    }

    void apply_errorThenConnected_announcesConnected()
    {
        // The VPN is up after all (a disconnect that failed, or a connect made
        // with the CLI): that is news, and replaces the error.
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Error, QString(), connectedTo(nj()));

        QCOMPARE(outcome.state, VpnState::Connected);
        QCOMPARE(outcome.server, nj());
        QCOMPARE(outcome.stateChanged, true);
    }

    //  The server of a connection the app made

    void apply_connectedWithoutServer_learnsServerWithoutNewState()
    {
        // After `protonvpn connect` the app is Connected but has no server
        // yet; the first poll supplies it.
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Connected, QString(), connectedTo(nj()));

        QCOMPARE(outcome.state, VpnState::Connected);
        QCOMPARE(outcome.server, nj());
        QCOMPARE(outcome.stateChanged, false);
        QCOMPARE(outcome.serverLearned, true);
        QCOMPARE(outcome.serverChanged, false);
    }

    void apply_connectedSameServer_isNoNews()
    {
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Connected, nj(), connectedTo(nj()));

        QCOMPARE(outcome.stateChanged, false);
        QCOMPARE(outcome.serverLearned, false);
        QCOMPARE(outcome.serverChanged, false);
        QCOMPARE(outcome.server, nj());
    }

    void apply_connectedOtherServer_reportsServerChange()
    {
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Connected, nj(), connectedTo(ch()));

        QCOMPARE(outcome.state, VpnState::Connected);
        QCOMPARE(outcome.server, ch());
        QCOMPARE(outcome.stateChanged, false);
        QCOMPARE(outcome.serverChanged, true);
        QCOMPARE(outcome.serverLearned, false);
    }

    void apply_connectedWithoutServerLine_keepsKnownServer()
    {
        const QMap<QString, QString> fields{{QStringLiteral("status"), QStringLiteral("Connected")}};
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Connected, nj(), fields);

        QCOMPARE(outcome.server, nj());
        QCOMPARE(outcome.serverChanged, false);
    }

    //  No server while not connected

    void apply_disconnectedWithLeftoverServer_forgetsIt()
    {
        // Disconnecting in the app left the old server behind; a poll that
        // agrees the VPN is down must not leave it there (the D-Bus interface
        // reports it).
        const StatusSnapshot::Outcome outcome = StatusSnapshot::apply(VpnState::Disconnected, nj(), disconnected());

        QCOMPARE(outcome.stateChanged, false);
        QVERIFY(outcome.server.isEmpty());
    }
};

QTEST_MAIN(TstStatusSnapshot)
#include "tst_statusSnapshot.moc"
