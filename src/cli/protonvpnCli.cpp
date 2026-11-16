// protonvpnCli.cpp
// All VpnManager methods that interact with the protonvpn CLI by spawning
// a QProcess.  State management, the settings file, and polling
// infrastructure live in vpnManager.cpp.

#include "../vpnManager.h"

#include "../debug.h"
#include "cliDisconnect.h"
#include "cliInstallCheck.h"
#include "cliNoiseFilter.h"
#include "cliSession.h"
#include "cliSettings.h"
#include "cliTable.h"
#include "cliVersion.h"
#include "logRedaction.h"
#include "flatpakUtils.h"
#include "statusMonitor.h"
#include "signin/signinFlows.h"
#include "../uiHelpers.h"

#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QVersionNumber>
#include <algorithm>
#include <csignal>
#include <functional>
#include <memory>
#include <ranges>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Flatpak helper - when running inside a Flatpak sandbox, all CLI calls must
// be forwarded to the host via flatpak-spawn.
// ---------------------------------------------------------------------------

namespace
{
constexpr int DISCONNECT_SYNC_TIMEOUT_MS = 10000;
constexpr int LOGIN_CHECK_RETRIES = 5;
constexpr int MIN_COUNTRY_PARTS          = 2;
constexpr int NETWORK_READY_CHECK_RETRIES = 5;
constexpr int AUTO_CONNECT_RETRIES        = 5;
// A connect that is tried once (every connect but the startup auto-connect).
constexpr int NO_RETRIES                  = 0;
// Retries back off linearly: the first waits this long, the second twice as
// long, and so on.
constexpr int RETRY_BACKOFF_STEP_MS       = 1000;

// `protonvpn info` and `protonvpn config list` normally answer in well under a
// second. The limit is generous because the CLI may be legitimately waiting
// on a keyring unlock prompt the user is still typing into.
constexpr int KEYRING_PROMPT_TIMEOUT_MS = 30000;
// `protonvpn` without a command only prints its banner, in well under a
// second; sign-in reads its version from it.
constexpr int VERSION_CHECK_TIMEOUT_MS = 10000;
// How long a CLI that was asked to stop (a timeout, or a canceled sign-in)
// gets to exit before it is killed.
constexpr int CLI_TERMINATE_GRACE_MS = 2000;
// Exit codes runCommand() reports when the CLI never produced one of its own.
// Negative, so they can never collide with a real process exit status.
constexpr int CLI_EXIT_FAILED_TO_START = -1;
constexpr int CLI_EXIT_TIMED_OUT       = -2;
// A sign-in CLI ended by a signal: its exit code is then the signal number.
constexpr int CLI_EXIT_CRASHED         = -3;
// Where the username is in every sign-in flow's arguments ("signin <username> ...").
constexpr int SIGNIN_USERNAME_ARG = 1;

// Returns {program, fullArgs} ready for QProcess::start.
std::pair<QString, QStringList> buildCliCommand(const QStringList& args)
{
    return buildHostCommand(QStringLiteral("protonvpn"), args);
}

// The rows of `protonvpn cities list` as (city, features), e.g.
// {"Zurich", "P2P, Tor"}; features are empty for a city without any.
QList<QPair<QString, QString>> parseCities(const QString& output)
{
    QList<QPair<QString, QString>> cities;
    for (const QString& row : CliTable::rows(output))
    {
        const QStringList cells = CliTable::cells(row);
        cities.append({cells.value(0), cells.value(1)});
    }
    return cities;
}
} // namespace

// ---------------------------------------------------------------------------
// Internal helper - run any protonvpn sub-command asynchronously.
// ---------------------------------------------------------------------------

void VpnManager::runCommand(const QStringList& args,
                            const std::function<void(int, const QString&, const QString&)>& callback,
                            int timeoutMs)
{
    QProcess* process = new QProcess(this);
    const QString cmdLine = QStringLiteral("protonvpn ") + args.join(QLatin1Char(' '));
    // `signout` itself may fail on a rejected session; it must not trigger
    // another sign-out.
    const bool isSignOut = args.value(0) == QLatin1String("signout");
    DBG_CLI(QStringLiteral(">>> ") + cmdLine);

    // Set when the watchdog gives up on the process, so finished() reports a
    // timeout rather than the exit code of the signal that ended it.
    std::shared_ptr<bool> timedOut = std::make_shared<bool>(false);
    QTimer* watchdog = nullptr;
    if (timeoutMs != CLI_NO_TIMEOUT)
    {
        watchdog = new QTimer(process);
        watchdog->setSingleShot(true);
        connect(watchdog, &QTimer::timeout, process, [process, timedOut, cmdLine, timeoutMs]()
        {
            *timedOut = true;
            DBG_CLI(QStringLiteral("No response from '%1' after %2 ms - terminating it.")
                        .arg(cmdLine).arg(timeoutMs));
            // SIGTERM first: under Flatpak the child is flatpak-spawn, which
            // forwards SIGTERM to the real CLI on the host, whereas SIGKILL
            // would only kill flatpak-spawn and leave the CLI running.
            process->terminate();
            QTimer::singleShot(CLI_TERMINATE_GRACE_MS, process, [process]()
            {
                if (process->state() != QProcess::NotRunning)
                {
                    process->kill();
                }
            });
        });
        watchdog->start(timeoutMs);
    }

    // A program that cannot be started never emits finished(), so without this
    // the callback - and anything waiting on it - would never run.
    connect(process, &QProcess::errorOccurred,
            this, [process, callback, cmdLine, watchdog](QProcess::ProcessError error)
            {
                if (error != QProcess::FailedToStart) return;
                if (watchdog != nullptr)
                {
                    watchdog->stop();
                }
                DBG_CLI(QStringLiteral("<<< ") + cmdLine +
                        QStringLiteral(" [failed to start: ") + process->errorString() + QStringLiteral("]"));
                callback(CLI_EXIT_FAILED_TO_START, QString(), process->errorString());
                process->deleteLater();
            });

    connect(process, &QProcess::finished,
            this, [this, process, callback, cmdLine, isSignOut, watchdog, timedOut](int exitCode, QProcess::ExitStatus exitStatus)
            {
                if (watchdog != nullptr)
                {
                    watchdog->stop();
                }
                if (*timedOut == true)
                {
                    exitCode = CLI_EXIT_TIMED_OUT;
                }
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                // A process ended by a signal reports the signal number as its
                // exit code, which is indistinguishable from a real exit status
                // (signal 1 vs exit(1)) unless the crash is logged as such.
                QString outcome;
                if (*timedOut == true)
                {
                    outcome = QStringLiteral(" [timed out]");
                }
                else if (exitStatus == QProcess::CrashExit)
                {
                    outcome = QStringLiteral(" [crashed: signal ") + QString::number(exitCode) + QStringLiteral("]");
                }
                else
                {
                    outcome = QStringLiteral(" [exit=") + QString::number(exitCode) + QStringLiteral("]");
                }
                DBG_CLI(QStringLiteral("<<< ") + cmdLine + outcome);
                if (out.isEmpty() == false)
                {
                    DBG_CLI(QStringLiteral("    stdout: ") + LogRedaction::cliOutput(out));
                }
                if (err.isEmpty() == false)
                {
                    DBG_CLI(QStringLiteral("    stderr: ") + LogRedaction::cliOutput(err));
                }
                callback(exitCode, out, err);
                process->deleteLater();

                // `protonvpn info` keeps reporting the saved account after the
                // server has rejected the session, so whichever command first
                // gets a 401 is how the app finds out. Clearing m_signedIn makes
                // this fire once, even when several commands fail together.
                if (exitCode != 0 && isSignOut == false && m_signedIn == true &&
                    CliSession::requiresSignIn(out + QLatin1Char('\n') + err) == true)
                {
                    DBG_CLI(QStringLiteral("'") + cmdLine +
                            QStringLiteral("' reports the session is no longer valid - signing out."));
                    m_signedIn = false;
                    emit sessionExpired();
                }
            });
    auto [program, fullArgs] = buildCliCommand(args);
    process->start(program, fullArgs);
}

// ---------------------------------------------------------------------------
// Installation / login
// ---------------------------------------------------------------------------

void VpnManager::checkInstalled()
{
    DBG_CLI(QStringLiteral("Checking if protonvpn CLI is installed..."));
    QProcess* process = new QProcess(this);

    // Guard so exactly one of finished()/errorOccurred() reports the result.
    // The previous version blocked the GUI on waitForStarted() during startup
    // and could both report "not installed" and later delete a process that
    // was still starting.
    std::shared_ptr<bool> reported = std::make_shared<bool>(false);

    connect(process, &QProcess::finished,
            this, [this, process, reported](const int exitCode, QProcess::ExitStatus)
            {
                Q_UNUSED(exitCode)
                if (*reported == true)
                {
                    process->deleteLater();
                    return;
                }
                *reported = true;
                const QString out = QString::fromUtf8(process->readAllStandardOutput());
                const QString err = QString::fromUtf8(process->readAllStandardError());
                const bool installed = CliInstallCheck::ranCli(out, err);
                process->deleteLater();
                DBG_CLI(installed ? QStringLiteral("protonvpn CLI found.") : QStringLiteral("protonvpn CLI NOT found."));
                emit installedResult(installed);
            });

    connect(process, &QProcess::errorOccurred,
            this, [this, process, reported](QProcess::ProcessError error)
            {
                if (*reported == true) return;
                *reported = true;
                DBG_CLI(QStringLiteral("protonvpn CLI NOT found (QProcess::ProcessError=%1).")
                            .arg(error));
                process->deleteLater();
                emit installedResult(false);
            });

    auto [program, fullArgs] = buildCliCommand({QStringLiteral("--help")});
    process->start(program, fullArgs);
}

void VpnManager::checkLoginStatus()
{
    checkLoginStatus(LOGIN_CHECK_RETRIES);
}

void VpnManager::checkLoginStatus(int retriesLeft)
{
    runCommand({QStringLiteral("info")}, [this, retriesLeft](int exitCode, const QString& out, const QString&)
    {
        // A CLI that never answers is usually blocked on something only the
        // user can resolve (e.g. a keyring unlock prompt). Retrying on a timer
        // would just hold the app on "Starting..." for another full timeout.
        if (exitCode == CLI_EXIT_TIMED_OUT)
        {
            DBG_CLI(QStringLiteral("checkLoginStatus: protonvpn info did not respond within %1 ms.")
                        .arg(KEYRING_PROMPT_TIMEOUT_MS));
            emit loginCheckTimedOut();
            return;
        }

        // checkInstalled() found the CLI moments ago, but it can no longer be
        // started (e.g. it was uninstalled in the meantime).
        if (exitCode == CLI_EXIT_FAILED_TO_START)
        {
            emit installedResult(false);
            return;
        }

        const QRegularExpression re(QStringLiteral(R"(Account:\s*'([^']+)')"));
        const QRegularExpressionMatch match = re.match(out);

        if (match.hasMatch())
        {
            // CLI responded correctly - determine logged-in state from the value.
            const QString accountVal = match.captured(1).trimmed();
            const bool loggedIn = (accountVal != QStringLiteral("None") && accountVal.isEmpty() == false);
            if (loggedIn)
            {
                m_signedIn = true;
                startStatusMonitor();
                fetchSettings();
                emit loginStatusResult(true, accountVal);
            }
            else
            {
                // Explicit "Account: 'None'" - genuinely not logged in, no point retrying.
                m_signedIn = false;
                emit loginStatusResult(false, QString());
            }
            return;
        }

        // No "Account:" field in the output: the CLI didn't respond cleanly.
        // This is the transient case (daemon/keyring not ready on autostart).
        DBG_CLI(QStringLiteral("checkLoginStatus: no Account field (exitCode=%1), retries left: %2")
                    .arg(exitCode).arg(retriesLeft));

        if (retriesLeft > 0)
        {
            const int attempt = LOGIN_CHECK_RETRIES - retriesLeft + 1;
            const int delayMs = attempt * RETRY_BACKOFF_STEP_MS;
            DBG_CLI(QStringLiteral("Retrying in %1 ms...").arg(delayMs));
            QTimer::singleShot(delayMs, this, [this, retriesLeft]()
            {
                checkLoginStatus(retriesLeft - 1);
            });
        }
        else
        {
            m_signedIn = false;
            emit loginStatusResult(false, QString());
        }
    }, KEYRING_PROMPT_TIMEOUT_MS);
}

void VpnManager::login(const QString& username, const QString& password)
{
    DBG_CLI(QStringLiteral("Login attempt for user: ") + LogRedaction::username(username));
    endSignin();
    const int attempt = m_signinAttempt;

    // How `protonvpn signin` talks depends on the CLI's version, and the CLI
    // may have been updated since the app started, so the version is read
    // again here instead of taken from startup.
    runCommand({}, [this, attempt, username, password](int, const QString& out, const QString& err)
    {
        if (attempt != m_signinAttempt) return; // canceled, or another sign-in started
        const QString version = CliVersion::fromBanner(out + QLatin1Char('\n') + err);
        m_signinFlow = SigninFlows::forCliVersion(QVersionNumber::fromString(version));
        DBG_CLI(QStringLiteral("Signing in with CLI %1: %2.")
                    .arg(version.isEmpty() ? QStringLiteral("(version unknown)") : version,
                         signinFlowName(m_signinFlow->kind())));
        applySigninEffects(m_signinFlow->begin(username, password));
    }, VERSION_CHECK_TIMEOUT_MS);
}

void VpnManager::cancelLogin()
{
    DBG_CLI(QStringLiteral("Sign-in canceled."));
    endSignin();
}

void VpnManager::submit2FA(const QString& code)
{
    if (m_signinFlow == nullptr) return; // no sign-in in progress, e.g. a Debug page preview
    DBG_CLI(QStringLiteral("Submitting the two-factor code."));
    applySigninEffects(m_signinFlow->submitCode(code));
}

void VpnManager::submitSecurityKeyPin(const QString& pin)
{
    if (m_signinFlow == nullptr) return;
    DBG_CLI(QStringLiteral("Submitting the security key PIN."));
    applySigninEffects(m_signinFlow->submitPin(pin));
}

void VpnManager::retrySecurityKey()
{
    if (m_signinFlow == nullptr) return;
    DBG_CLI(QStringLiteral("Trying the security key again."));
    applySigninEffects(m_signinFlow->retrySecurityKey());
}

void VpnManager::useAuthenticatorCode()
{
    if (m_signinFlow == nullptr) return;
    DBG_CLI(QStringLiteral("Switching sign-in to an authenticator code."));
    applySigninEffects(m_signinFlow->useCodeInstead());
}

void VpnManager::applySigninEffects(const SigninEffects& effects)
{
    for (const SigninEffect& effect : effects)
    {
        switch (effect.type)
        {
            case SigninEffect::Type::Start:
                startSigninProcess(effect.arguments);
                break;

            case SigninEffect::Type::Write:
                // Never logged: this is the password, a code, or a PIN.
                if (m_signinProcess != nullptr)
                {
                    m_signinProcess->write(effect.text.toUtf8());
                }
                break;

            case SigninEffect::Type::Prompt:
                DBG_CLI(QStringLiteral("Sign-in is waiting for: ") + signinPromptName(effect.prompt) +
                        (effect.message.isEmpty() ? QString() : QStringLiteral(" (") + effect.message + QStringLiteral(")")));
                emit signinPrompt(effect.prompt, effect.message);
                break;

            case SigninEffect::Type::Finish:
                finishSignin(effect.ok, effect.message);
                return;
        }
    }
}

void VpnManager::startSigninProcess(const QStringList& arguments)
{
    stopSigninProcess();

    QProcess* process = new QProcess(this);
    m_signinProcess = process;

    // When the Qt app is launched from a terminal, child processes inherit the
    // controlling terminal.  Python's getpass.getpass() then opens /dev/tty
    // directly, bypassing the QProcess stdin pipe, so the password prompt appears
    // on the user's terminal rather than being captured here.
    // setsid() in the child (between fork and exec) creates a new session with
    // no controlling terminal, so getpass falls back to writing the prompt to
    // stderr and reading the answer from stdin, both of which QProcess pipes.
    // stopSigninProcess() stops the CLI with SIGINT, which the CLI would
    // ignore if the app was started with it ignored (as a script's background
    // job is): ignored signals are inherited.
    process->setChildProcessModifier([]()
    {
        ::setsid();
        ::signal(SIGINT, SIG_DFL);
    });

    // Standard output and error go to the flow in the order they arrive: the
    // CLI prints some prompts on one and some on the other.
    const auto deliver = [this, process](const QByteArray& bytes)
    {
        if (process != m_signinProcess || m_signinFlow == nullptr) return;
        applySigninEffects(m_signinFlow->onOutput(QString::fromUtf8(bytes)));
    };
    connect(process, &QProcess::readyReadStandardOutput, this, [process, deliver]()
    {
        deliver(process->readAllStandardOutput());
    });
    connect(process, &QProcess::readyReadStandardError, this, [process, deliver]()
    {
        deliver(process->readAllStandardError());
    });

    const auto ended = [this, process](const int exitCode)
    {
        process->deleteLater();
        if (process != m_signinProcess) return; // replaced or stopped on purpose
        m_signinProcess = nullptr;
        DBG_CLI(QStringLiteral("<<< protonvpn signin [exit=%1]").arg(exitCode));
        if (m_signinFlow != nullptr)
        {
            applySigninEffects(m_signinFlow->onFinished(exitCode));
        }
    };
    connect(process, &QProcess::finished, this, [ended](const int exitCode, const QProcess::ExitStatus status)
    {
        ended(status == QProcess::NormalExit ? exitCode : CLI_EXIT_CRASHED);
    });
    // A CLI that cannot be started never emits finished().
    connect(process, &QProcess::errorOccurred, this, [ended](const QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart)
        {
            ended(CLI_EXIT_FAILED_TO_START);
        }
    });

    QStringList logged = arguments;
    if (logged.size() > SIGNIN_USERNAME_ARG)
    {
        logged[SIGNIN_USERNAME_ARG] = LogRedaction::username(logged[SIGNIN_USERNAME_ARG]);
    }
    DBG_CLI(QStringLiteral(">>> protonvpn ") + logged.join(QLatin1Char(' ')));

    auto [program, fullArgs] = buildCliCommand(arguments);
    process->start(program, fullArgs);
}

void VpnManager::stopSigninProcess()
{
    QProcess* process = m_signinProcess;
    m_signinProcess = nullptr; // its output and exit are ignored from here on
    if (process == nullptr) return;

    if (process->state() != QProcess::Running || process->processId() <= 0)
    {
        // Not started yet (or already gone): nothing to interrupt.
        process->kill();
        if (process->state() == QProcess::NotRunning)
        {
            process->deleteLater();
        }
        return;
    }

    // SIGINT, as Ctrl+C would send in a terminal: the CLI stops reading the
    // security key and exits on its own (flatpak-spawn forwards the signal to
    // the CLI on the host). Killed if it is still running after a moment.
    ::kill(static_cast<pid_t>(process->processId()), SIGINT);
    QTimer::singleShot(CLI_TERMINATE_GRACE_MS, process, [process]()
    {
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
        }
    });
}

void VpnManager::endSignin()
{
    ++m_signinAttempt;
    stopSigninProcess();
    m_signinFlow.reset();
}

void VpnManager::finishSignin(const bool ok, const QString& error)
{
    DBG_CLI(ok == true ? QStringLiteral("Login succeeded.") : QStringLiteral("Login failed: ") + error);
    endSignin();
    if (ok == true)
    {
        m_signedIn = true;
        startStatusMonitor();
        fetchSettings();
    }
    emit loginFinished(ok, error);
}

void VpnManager::signOut()
{
    DBG_CLI(QStringLiteral("Signing out..."));
    m_signedIn = false;
    // Whatever connect is waiting or running belongs to the session that is
    // ending: its result is dropped and nothing held is started.
    m_connectQueue.supersede();
    m_connectQueue.dropHeld();
    stopStatusMonitor();
    runCommand({QStringLiteral("signout")}, [this](int exitCode, const QString&, const QString&)
    {
        DBG_CLI(exitCode == 0 ? QStringLiteral("Sign-out succeeded.") : QStringLiteral("Sign-out failed (exit=") + QString::number(exitCode) + QStringLiteral(")."));
        emit signOutFinished(exitCode == 0);
        // Announce the state, not just record it. The pages still show the last
        // state they were sent - usually Error, when an expired session is what
        // triggered this sign-out - and after the next sign-in the status
        // monitor reports Disconnected, which already matches m_state, so no
        // change would be emitted and the stale error would stay on screen.
        announceState(VpnState::Disconnected, QString());
    });
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void VpnManager::connectVpn(const QString& country, const QString& city)
{
    beginConnect({.country = country, .city = city, .server = QString()});
}

void VpnManager::beginConnect(const ConnectRequest& request)
{
    // Calls off whatever an earlier request still has waiting (a startup
    // auto-connect's network check or retry).
    const int ticket = m_connectQueue.supersede();
    enterConnecting(QStringLiteral("Connecting to VPN"), request.country, request.city, request.server);

    if (m_connectQueue.commandRunning() == true)
    {
        // One CLI connect or disconnect at a time: this one starts when the
        // running one has finished, in place of announcing its result.
        DBG_CLI(QStringLiteral("A connect or disconnect is still running - this connect starts when it has finished."));
        m_connectQueue.hold(request);
        return;
    }
    issueConnect(request, NO_RETRIES, ticket);
}

bool VpnManager::startHeldConnect()
{
    const std::optional<ConnectRequest> held = m_connectQueue.takeHeld();
    if (held.has_value() == false) return false;

    // beginConnect() already recorded the destination and entered Connecting.
    issueConnect(*held, NO_RETRIES, m_connectQueue.supersede());
    return true;
}

void VpnManager::enterConnecting(const QString& action, const QString& country, const QString& city,
                                 const QString& server)
{
    if (server.isEmpty() == false)
    {
        DBG_CLI(action + QStringLiteral(" - server: '") + server + QStringLiteral("'"));
    }
    else
    {
        DBG_CLI(action + QStringLiteral(" - country: '") + (country.isEmpty() ? QStringLiteral("(fastest)") : country) +
                QStringLiteral("'  city: '") + (city.isEmpty() ? QStringLiteral("(any)") : city) + QStringLiteral("'"));
    }
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;
    m_lastConnectServer  = server;

    announceState(VpnState::Connecting, QString());
}

void VpnManager::startupAutoConnect(const QString& country, const QString& city)
{
    const int ticket = m_connectQueue.supersede();
    enterConnecting(QStringLiteral("Auto-connecting to VPN on startup"), country, city, QString());

    checkNetworkReady(NETWORK_READY_CHECK_RETRIES, [this, country, city, ticket]()
    {
        // The user connected somewhere, or disconnected, while the network
        // was coming up: that request stands and this one is dropped.
        if (m_connectQueue.isCurrent(ticket) == false)
        {
            DBG_CLI(QStringLiteral("Startup auto-connect dropped: a newer request replaced it."));
            return;
        }
        issueConnect({.country = country, .city = city, .server = QString()}, AUTO_CONNECT_RETRIES, ticket);
    });
}

void VpnManager::checkNetworkReady(int retriesLeft, const std::function<void()>& onReady)
{
    QProcess* process = new QProcess(this);

    // Guard so onReady() runs exactly once: a launch failure and a late finish
    // must not both fire it, which would issue the connect twice.
    std::shared_ptr<bool> reported = std::make_shared<bool>(false);

    connect(process, &QProcess::finished, this,
            [this, process, retriesLeft, onReady, reported](int exitCode, QProcess::ExitStatus)
    {
        if (*reported == true)
        {
            process->deleteLater();
            return;
        }
        *reported = true;
        const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        process->deleteLater();

        // nmcli terse "STATE" is e.g. "connected", "connecting", "disconnected",
        // "asleep" - only the "connected*" family means NetworkManager has a
        // default route up. startsWith() (not contains()) so "disconnected"
        // does not falsely match.
        const bool ready = exitCode == 0 && out.startsWith(QStringLiteral("connected"));
        if (ready || retriesLeft <= 0)
        {
            onReady();
            return;
        }

        const int attempt = NETWORK_READY_CHECK_RETRIES - retriesLeft + 1;
        const int delayMs = attempt * RETRY_BACKOFF_STEP_MS;
        DBG_CLI(QStringLiteral("Network not ready yet (nmcli state: '") + out +
                QStringLiteral("'), retrying in ") + QString::number(delayMs) + QStringLiteral(" ms..."));
        QTimer::singleShot(delayMs, this, [this, retriesLeft, onReady]()
        {
            checkNetworkReady(retriesLeft - 1, onReady);
        });
    });

    // nmcli missing or failed to launch - fail open rather than block
    // auto-connect indefinitely on a system without NetworkManager/nmcli.
    connect(process, &QProcess::errorOccurred, this,
            [process, onReady, reported](QProcess::ProcessError error)
    {
        if (*reported == true) return;
        *reported = true;
        DBG_CLI(QStringLiteral("nmcli could not be launched (QProcess::ProcessError=%1) - "
                               "proceeding without a network-ready check.").arg(error));
        process->deleteLater();
        onReady();
    });

    auto [program, fullArgs] = buildHostCommand(QStringLiteral("nmcli"),
        {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("STATE"),
         QStringLiteral("general"), QStringLiteral("status")});
    process->start(program, fullArgs);
}

void VpnManager::issueConnect(const ConnectRequest& request, const int retriesLeft, const int ticket)
{
    QStringList args{QStringLiteral("connect")};
    if (request.server.isEmpty() == false)
    {
        args << request.server;
    }
    else
    {
        if (request.country.isEmpty() == false)
        {
            args << QStringLiteral("--country") << request.country;
        }
        if (request.city.isEmpty() == false)
        {
            args << QStringLiteral("--city") << request.city;
        }
    }

    m_connectQueue.commandStarted();
    runCommand(args, [this, request, retriesLeft, ticket](int exitCode, const QString& out, const QString& err)
    {
        m_connectQueue.commandFinished();

        // A newer connect was asked for meanwhile: it replaces this result.
        if (startHeldConnect() == true) return;
        // Replaced by a disconnect or a sign-out, which report for themselves.
        if (m_connectQueue.isCurrent(ticket) == false) return;

        if (exitCode == 0)
        {
            DBG_CLI(QStringLiteral("VPN connected successfully."));
            // Strip noise / port-forwarding guidelines from CLI output.
            QStringList lines = CliNoise::strip(out.split(QLatin1Char('\n')));

            while (lines.isEmpty() == false && lines.first().trimmed().isEmpty())
            {
                lines.removeFirst();
            }

            announceState(VpnState::Connected, lines.join(QLatin1Char('\n')));
            refreshStatusNow();
            return;
        }

        // Retrying cannot fix a missing session; runCommand() has already
        // reported it, and the sign-out it triggers takes over from here.
        if (retriesLeft > 0 && CliSession::requiresSignIn(out + QLatin1Char('\n') + err) == false)
        {
            const int attempt = AUTO_CONNECT_RETRIES - retriesLeft + 1;
            const int delayMs = attempt * RETRY_BACKOFF_STEP_MS;
            DBG_CLI(QStringLiteral("VPN connect attempt failed (exit=") + QString::number(exitCode) +
                    QStringLiteral("), retrying in ") + QString::number(delayMs) + QStringLiteral(" ms..."));
            QTimer::singleShot(delayMs, this, [this, request, retriesLeft, ticket]()
            {
                // Replaced while waiting to retry.
                if (m_connectQueue.isCurrent(ticket) == false) return;
                issueConnect(request, retriesLeft - 1, ticket);
            });
            return;
        }

        const QString error = err.isEmpty() ? out : err;
        DBG_CLI(QStringLiteral("VPN connection failed (exit=") + QString::number(exitCode) + QStringLiteral("): ") + error);
        announceState(VpnState::Error, error);
        refreshStatusNow();
    });
}

void VpnManager::disconnectVpn()
{
    DBG_CLI(QStringLiteral("Disconnecting VPN..."));
    // Calls off a connect that is still waiting; one that is held is dropped.
    const int ticket = m_connectQueue.supersede();
    m_connectQueue.dropHeld();
    announceState(VpnState::Disconnecting, QString());

    m_connectQueue.commandStarted();
    runCommand({QStringLiteral("disconnect")}, [this, ticket](const int exitCode, const QString& out, const QString& err)
    {
        m_connectQueue.commandFinished();

        // A connect was asked for while disconnecting: go straight on to it.
        if (startHeldConnect() == true) return;
        // Signed out meanwhile; the sign-out announces the state.
        if (m_connectQueue.isCurrent(ticket) == false) return;

        if (CliDisconnect::succeeded(exitCode, out) == true)
        {
            if (exitCode != 0)
            {
                DBG_CLI(QStringLiteral("CLI confirmed the disconnect but exited with code ") +
                        QString::number(exitCode) + QStringLiteral("; treating it as successful."));
            }
            DBG_CLI(QStringLiteral("VPN disconnected successfully."));
            announceState(VpnState::Disconnected, out);
        }
        else
        {
            const QString error = err.isEmpty() ? out : err;
            DBG_CLI(QStringLiteral("VPN disconnect failed (exit=") + QString::number(exitCode) + QStringLiteral("): ") + error);
            announceState(VpnState::Error, error);
        }
        refreshStatusNow();
    });
}

void VpnManager::disconnectVpnSync()
{
    // Used at application exit - run synchronously so the event loop does not
    // need to stay alive.
    QProcess process;
    auto [program, fullArgs] = buildCliCommand({QStringLiteral("disconnect")});
    process.start(program, fullArgs);
    process.waitForFinished(DISCONNECT_SYNC_TIMEOUT_MS); // up to 10 s
}

void VpnManager::applyConfigValueAndReconnect(const QString& key, const QString& value)
{
    // Back to the same place. That is what the app last asked for when it
    // describes this connection; when it does not - the connection was
    // already up when the app started, was made with the CLI, or went to the
    // fastest server anywhere - it is the same server, by name. Reconnecting
    // by country instead would land on whatever is fastest there now, and
    // could drop a P2P, Secure Core, or Tor server the CLI was asked for.
    QString country = m_lastConnectCountry;
    QString city    = m_lastConnectCity;
    QString server  = m_lastConnectServer;
    const QString connectedName    = StatusMonitor::parseServerName(m_connectedServer);
    const QString connectedCountry = StatusMonitor::parseCountryFromServer(m_connectedServer);
    const bool requestDescribesConnection = connectedName.isEmpty() == true
        || (server.isEmpty() == false && server.compare(connectedName, Qt::CaseInsensitive) == 0)
        || (server.isEmpty() == true && country.isEmpty() == false
            && country.compare(connectedCountry, Qt::CaseInsensitive) == 0);
    if (requestDescribesConnection == false)
    {
        server  = connectedName;
        country = connectedCountry; // for the UI while connecting; the name decides
        city.clear();
    }

    const int ticket = m_connectQueue.supersede();
    m_connectQueue.dropHeld();
    announceState(VpnState::Disconnecting, QString());

    m_connectQueue.commandStarted();
    runCommand({QStringLiteral("disconnect")},
               [this, key, value, country, city, server, ticket](int exitCode, const QString& out, const QString& err)
    {
        // Replaced by a sign-out or a plain disconnect, which report for
        // themselves. A connect that is being held is different: the setting
        // is still applied, and that connect follows instead of the reconnect.
        if (m_connectQueue.isCurrent(ticket) == false && m_connectQueue.hasHeld() == false)
        {
            m_connectQueue.commandFinished();
            return;
        }

        if (CliDisconnect::succeeded(exitCode, out) == false)
        {
            m_connectQueue.commandFinished();
            const QString error = err.isEmpty() ? out : err;
            DBG_CLI(QStringLiteral("Disconnect before changing '") + key + QStringLiteral("' failed (exit=") +
                    QString::number(exitCode) + QStringLiteral("): ") + error);
            // A connect asked for meanwhile still goes ahead.
            if (startHeldConnect() == true) return;
            announceState(VpnState::Error, error);
            refreshStatusNow();
            return;
        }

        if (m_connectQueue.hasHeld() == false)
        {
            announceState(VpnState::Disconnected, QString());
        }

        QStringList args{QStringLiteral("config"), QStringLiteral("set")};
        args << key;
        args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        // Still one command at a time: the queue stays busy through the
        // setting change, so a connect asked for now waits for it too.
        runCommand(args, [this, country, city, server, ticket](int, const QString& out2, const QString& err2)
        {
            m_connectQueue.commandFinished();
            emit configApplied((out2 + QLatin1Char('\n') + err2).trimmed());
            fetchSettings();

            // The user picked another location meanwhile: that one, not the
            // previous connection.
            if (startHeldConnect() == true) return;
            if (m_connectQueue.isCurrent(ticket) == false) return;
            beginConnect({.country = country, .city = city, .server = server});
        });
    });
}

// ---------------------------------------------------------------------------
// Server / city / account queries
// ---------------------------------------------------------------------------

void VpnManager::fetchCountries()
{
    runCommand({QStringLiteral("countries"), QStringLiteral("list")},
               [this](int exitCode, const QString& out, const QString& err)
    {
        if (exitCode != 0)
        {
            emit countriesFetchFailed();
            return;
        }
        QMap<QString, QString> countries;
        // Rows are "Name   Code".
        for (const QString& row : CliTable::rows(out + QLatin1Char('\n') + err))
        {
            const QStringList cells = CliTable::cells(row);
            if (cells.size() < MIN_COUNTRY_PARTS) continue;
            countries.insert(cells.first(), cells.last());
        }
        m_countries = countries;
        emit countriesReady(countries);
    });
}

void VpnManager::fetchCities(const QString& countryCode)
{
    runCommand({QStringLiteral("cities"), QStringLiteral("list"), countryCode},
               [this, countryCode](int exitCode, const QString& out, const QString& err)
               {
                   if (exitCode != 0)
                   {
                       // Not authenticated or another error: the cached list,
                       // if any, is not overwritten with an empty one.
                       emit citiesFetchFailed(countryCode);
                       return;
                   }
                   const QList<QPair<QString, QString>> cities = parseCities(out + QLatin1Char('\n') + err);
                   m_cities.insert(countryCode, cities);
                   emit citiesReady(countryCode, cities);
               });
}

void VpnManager::fetchCityFeatures(const QString& countryCode, const QString& city,
                                   const std::function<void(const QString& features)>& callback)
{
    runCommand({QStringLiteral("cities"), QStringLiteral("list"), countryCode},
               [city, callback](int exitCode, const QString& out, const QString& err)
               {
                   if (exitCode != 0) return;
                   for (const auto& [name, features] : parseCities(out + QLatin1Char('\n') + err))
                   {
                       if (name.compare(city, Qt::CaseInsensitive) == 0)
                       {
                           callback(features);
                           return;
                       }
                   }
                   callback(QString()); // city not found
               });
}

void VpnManager::fetchInfo()
{
    runCommand({QStringLiteral("info")}, [this](int, const QString& out, const QString&)
    {
        QMap<QString, QString> result;
        const QRegularExpression re(QStringLiteral(R"((\w[\w ]*):\s*'([^']*)')"));
        QRegularExpressionMatchIterator it = re.globalMatch(out);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            result.insert(m.captured(1).trimmed(), m.captured(2).trimmed());
        }
        emit infoReady(result);
    });
}

void VpnManager::fetchCliVersion()
{
    // "protonvpn" in the terminal with no args prints the ASCII banner; the version number
    // (semver) appears on the last banner line.
    runCommand({}, [this](int, const QString& out, const QString& err)
    {
        emit cliVersionReady(CliVersion::fromBanner(out + QLatin1Char('\n') + err));
    });
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void VpnManager::applyConfig(const QString& key, const bool enabled)
{
    applyConfigValue(key, onOffString(enabled));
}

void VpnManager::applyConfigValue(const QString& key, const QString& value)
{
    DBG_CLI(QStringLiteral("Applying CLI config: ") + key + QStringLiteral(" = ") + value);
    QStringList args{QStringLiteral("config"), QStringLiteral("set"), key};
    args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    ++m_configSetsInFlight;
    runCommand(args, [this](int, const QString& out, const QString& err)
    {
        const QString combined = (out + QLatin1Char('\n') + err).trimmed();
        emit configApplied(combined);
        // Re-read once the last of several quick changes is in, so a reply
        // from an earlier one cannot briefly undo a later one on screen. This
        // also puts a rejected change back the way the CLI has it.
        --m_configSetsInFlight;
        if (m_configSetsInFlight == 0)
        {
            fetchSettings();
        }
    });
}

void VpnManager::fetchSettings()
{
    runCommand({QStringLiteral("config"), QStringLiteral("list")},
               [this](const int exitCode, const QString& out, const QString& err)
    {
        QMap<QString, QString> settings = CliSettings::parseConfigList(out);
        const bool answered = exitCode == 0 && settings.isEmpty() == false;
        if (answered == true)
        {
            // The same output tells the plan, so no second `config list` is
            // run for it. A CLI that failed says nothing about the plan, and
            // the account type stays what it was.
            m_accountType = CliSettings::offersPlusUpgrade(out + QLatin1Char('\n') + err) == true
                            ? AccountType::Free
                            : AccountType::Plus;
            emit accountTypeReady(accountType());
        }
        if (answered == false)
        {
            // The CLI could not answer (not signed in, or it failed): the file
            // at least has every setting the user has changed.
            DBG_CLI(QStringLiteral("Could not read settings from the CLI - using its settings file."));
            settings = readSettingsFile();
        }
        else
        {
            // Custom DNS reads "off", "on", or "on  [1.1.1.1, 8.8.8.8, ...]";
            // the Settings page wants "off", "on", or the full server list.
            const QString dnsKey = QStringLiteral("custom-dns");
            const QString dns = settings.value(dnsKey);
            if (dns.startsWith(QLatin1String("on")) == true)
            {
                bool truncated = false;
                QStringList servers = CliSettings::customDnsServers(dns, &truncated);
                if (truncated == true)
                {
                    const QStringList saved = readSettingsFile().value(dnsKey).split(QLatin1Char(','));
                    if (saved.size() > servers.size())
                    {
                        servers = saved;
                    }
                }
                settings.insert(dnsKey, CliSettings::customDnsValue(servers));
            }
        }
        m_settings = settings;
        emit settingsReady(settings);
    }, KEYRING_PROMPT_TIMEOUT_MS);
}

