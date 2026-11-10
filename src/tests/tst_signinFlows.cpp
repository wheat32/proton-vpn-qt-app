#include <QtTest/QtTest>
#include "cli/cliVersion.h"
#include "cli/signin/signinFlows.h"
#include "cliBanner.h"
#include "signinTestUtils.h"

// SigninFlows picks the sign-in conversation for the installed CLI. A wrong
// pick breaks signing in for everyone on that CLI: the legacy flow cannot
// follow a security key, and the security key flow would offer --totp to a
// CLI that rejects it.

using namespace SigninTest;

class TstSigninFlows : public QObject
{
    Q_OBJECT

private:
    static void addVersionRows()
    {
        QTest::addColumn<QString>("version");
        QTest::addColumn<SigninFlowKind>("expected");

        QTest::newRow("1.0.0") << QStringLiteral("1.0.0") << SigninFlowKind::AuthenticatorCode;
        QTest::newRow("1.0.1") << QStringLiteral("1.0.1") << SigninFlowKind::AuthenticatorCode;
        QTest::newRow("1.0.2") << QStringLiteral("1.0.2") << SigninFlowKind::AuthenticatorCode;
        QTest::newRow("1.0.3") << QStringLiteral("1.0.3") << SigninFlowKind::AuthenticatorCode;
        QTest::newRow("1.0.4") << QStringLiteral("1.0.4") << SigninFlowKind::SecurityKey;
        QTest::newRow("1.0.5") << QStringLiteral("1.0.5") << SigninFlowKind::SecurityKey;
        QTest::newRow("1.1.0") << QStringLiteral("1.1.0") << SigninFlowKind::SecurityKey;
        QTest::newRow("1.0.10") << QStringLiteral("1.0.10") << SigninFlowKind::SecurityKey;
        QTest::newRow("1.1.0") << QStringLiteral("1.1.0") << SigninFlowKind::SecurityKey;
        QTest::newRow("2.0.0") << QStringLiteral("2.0.0") << SigninFlowKind::SecurityKey;
        QTest::newRow("unreadable") << QString() << SigninFlowKind::SecurityKey;
    }

private slots:
    void securityKeyCliVersion_is104()
    {
        QCOMPARE(SigninFlows::SECURITY_KEY_CLI_VERSION, QVersionNumber(1, 0, 4));
    }

    void kindForCliVersion_data()
    {
        addVersionRows();
    }

    void kindForCliVersion()
    {
        QFETCH(QString, version);
        QFETCH(SigninFlowKind, expected);
        QCOMPARE(SigninFlows::kindForCliVersion(QVersionNumber::fromString(version)), expected);
    }

    void forCliVersion_data()
    {
        addVersionRows();
    }

    void forCliVersion()
    {
        QFETCH(QString, version);
        QFETCH(SigninFlowKind, expected);
        const std::unique_ptr<SigninFlow> flow = SigninFlows::forCliVersion(QVersionNumber::fromString(version));
        QVERIFY(flow != nullptr);
        QCOMPARE(flow->kind(), expected);
    }

    // The path VpnManager::login takes: banner, version, flow.
    void bannerVersion_data()
    {
        addVersionRows();
    }

    void bannerVersion()
    {
        QFETCH(QString, version);
        QFETCH(SigninFlowKind, expected);
        const QString read = CliVersion::fromBanner(CliBannerTest::banner(version));
        QCOMPARE(read, version);
        QCOMPARE(SigninFlows::forCliVersion(QVersionNumber::fromString(read))->kind(), expected);
    }

    //  Each pick behaves as its CLI needs

    void cli103_useCodeInstead_neverPassesTotp()
    {
        // CLI 1.0.3 has no --totp; it would refuse the option.
        const std::unique_ptr<SigninFlow> flow = SigninFlows::forCliVersion(QVersionNumber(1, 0, 3));
        QCOMPARE(describe(flow->begin(USERNAME, PASSWORD)), QStringList{START});
        QCOMPARE(describe(flow->useCodeInstead()), QStringList());
        QCOMPARE(describe(flow->retrySecurityKey()), QStringList());
        QCOMPARE(describe(flow->submitPin(PIN)), QStringList());
    }

    void cli103_codeConversation_signsIn()
    {
        const std::unique_ptr<SigninFlow> flow = SigninFlows::forCliVersion(QVersionNumber(1, 0, 3));
        QCOMPARE(describe(flow->begin(USERNAME, PASSWORD)), QStringList{START});
        QCOMPARE(feed(*flow, {PASSWORD_PROMPT, GETPASS_CODE_PROMPT}), QStringList({WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow->submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(*flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow->onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void cli105_keyConversation_showsKeyThenSwitchesToCode()
    {
        const std::unique_ptr<SigninFlow> flow = SigninFlows::forCliVersion(QVersionNumber(1, 0, 5));
        QCOMPARE(describe(flow->begin(USERNAME, PASSWORD)), QStringList{START});
        QCOMPARE(feed(*flow, {PASSWORD_PROMPT, WAITING_FOR_KEY}), QStringList({WRITE_PASSWORD, SHOW_KEY}));
        QCOMPARE(describe(flow->useCodeInstead()), QStringList{START_TOTP});
        QCOMPARE(feed(*flow, {PASSWORD_PROMPT, CODE_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList({WRITE_PASSWORD, SHOW_CODE}));
    }

    //  Names in logs

    void signinPromptName_everyPrompt_hasUniqueName()
    {
        const QList<SigninPrompt> prompts = {
            SigninPrompt::Code,
            SigninPrompt::SecurityKey,
            SigninPrompt::SecurityKeyTouch,
            SigninPrompt::SecurityKeyPin,
            SigninPrompt::SecurityKeyMissing,
            SigninPrompt::SecurityKeyChoose,
            SigninPrompt::SecurityKeyRead,
            SigninPrompt::SecurityKeyFailed,
        };
        QSet<QString> names;
        for (const SigninPrompt prompt : prompts)
        {
            const QString name = signinPromptName(prompt);
            QVERIFY(name.isEmpty() == false);
            names.insert(name);
        }
        QCOMPARE(names.size(), prompts.size());
    }

    void signinFlowName_eachKind_namesItsVersions()
    {
        QVERIFY(signinFlowName(SigninFlowKind::AuthenticatorCode).contains(QStringLiteral("1.0.3")));
        QVERIFY(signinFlowName(SigninFlowKind::SecurityKey).contains(QStringLiteral("1.0.4")));
    }
};

QTEST_MAIN(TstSigninFlows)
#include "tst_signinFlows.moc"
