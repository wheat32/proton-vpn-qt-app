#pragma once

#include <QProcess>
#include <QString>
#include <QMap>
#include <memory>
#include "cli/signin/signinFlow.h"

class StatusMonitor; // cli/statusMonitor.h - forward-declared to keep this
                     // header lightweight; full type used only in vpnManager.cpp.

enum class VpnState
{
    Unknown,
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

enum class AccountType
{
    Unknown,
    Free,
    Plus
};

class VpnManager : public QObject
{
    Q_OBJECT

public:
    explicit VpnManager(QObject* parent = nullptr);

    void checkInstalled();
    void checkLoginStatus();
    // Signs in with the conversation that fits the installed CLI (see
    // cli/signin/signinFlows.h); the second factor is asked for through
    // signinPrompt() and answered with the calls below.
    void login(const QString& username, const QString& password);
    void cancelLogin();
    void submit2FA(const QString& code);
    void submitSecurityKeyPin(const QString& pin);
    // Presses Enter at "insert your security key", or tries the key again after it failed.
    void retrySecurityKey();
    // Signs in again with an authenticator or recovery code instead of the security key.
    void useAuthenticatorCode();
    void signOut();
    void connectVpn(const QString& country = QString(), const QString& city = QString());
    // Like connectVpn(), but for the startup auto-connect path only: waits
    // for NetworkManager to report a ready state before the first attempt,
    // and retries with backoff if the connect attempt itself fails.
    void startupAutoConnect(const QString& country = QString(), const QString& city = QString());
    void disconnectVpn();
    static void disconnectVpnSync(); // blocking disconnect - safe to call just before app exit
    // Disconnect, change any config key/value, then reconnect to the previous
    // location: the country and city the app last connected to, or the same
    // server when the connection was not one the app made (see the .cpp).
    void applyConfigValueAndReconnect(const QString& key, const QString& value);
    void fetchCountries();
    void fetchCities(const QString& countryCode);
    // What the last fetchCountries() and each country's last fetchCities()
    // found, for a list that opens later (the auto-connect server chooser)
    // to start from without fetching again; empty until then.
    const QMap<QString, QString>& countries() const { return m_countries; } // name -> code
    const QMap<QString, QList<QPair<QString, QString>>>& cities() const { return m_cities; } // code -> (city, features)
    void fetchInfo();
    // Asks the CLI for the settings it is using (`protonvpn config list`) and
    // emits settingsReady(). Also runs on its own at sign-in and after every
    // applyConfigValue(), so listeners stay in step with the CLI.
    void fetchSettings();
    void applyConfig(const QString& key, bool enabled);
    void applyConfigValue(const QString& key, const QString& value);
    // Async: fetch the features string for a specific city in a country.
    // Parses `protonvpn cities list <countryCode>` and calls back with the
    // features string (e.g. "P2P, Tor") or an empty string if not found.
    void fetchCityFeatures(const QString& countryCode, const QString& city,
                           const std::function<void(const QString& features)>& callback);
    void fetchCliVersion();
    void fetchAccountType();

    VpnState    currentState()       const { return m_state; }
    AccountType accountType()        const { return m_accountType; }
    // Last country / city passed to connectVpn() - empty if connected via CLI.
    // After a reconnect to a server by name, the country is that server's.
    QString     lastConnectCountry() const { return m_lastConnectCountry; }
    QString     lastConnectCity()    const { return m_lastConnectCity; }
    // Last server string seen while Connected (e.g. "US-NJ#189") - empty otherwise.
    QString     connectedServer()    const { return m_connectedServer; }
    // The settings from the last fetchSettings() (setting -> value, e.g.
    // "ipv6" -> "on"), or from the CLI's settings file until the first one.
    QMap<QString, QString> settings() const { return m_settings; }
    bool        portForwardingEnabled() const;

signals:
    void installedResult(bool installed);
    void loginStatusResult(bool loggedIn, const QString& username);
    // Sign-in needs the user: a code, or something about the security key.
    // `error` is why the previous answer failed, when it did.
    void signinPrompt(SigninPrompt prompt, const QString& error);
    void loginFinished(bool ok, const QString& error);
    void signOutFinished(bool ok);
    void connectionStateChanged(VpnState state, const QString& info);
    // Emitted (before connectionStateChanged) when a city is parsed from
    // `protonvpn status` output, so the UI can pre-select it in the picker.
    void connectionCityKnown(const QString& city);
    // Emitted alongside connectionCityKnown with the 2-letter country code
    // extracted from the connected server name (e.g. "US" from "US-NJ#189").
    void connectionCountryKnown(const QString& countryCode);
    void countriesReady(const QMap<QString, QString>& countries); // name -> code
    void citiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities); // (city, features)
    void infoReady(const QMap<QString, QString>& info);
    void settingsReady(const QMap<QString, QString>& settings);
    void configApplied(const QString& output);
    void cliVersionReady(const QString& version);
    void accountTypeReady(AccountType type);
    void errorOccurred(const QString& error);
    // Emitted instead of loginStatusResult when `protonvpn info` does not exit
    // within the startup time limit - typically because the CLI is waiting on
    // a keyring/wallet unlock prompt.
    void loginCheckTimedOut();
    // Emitted (at most once per signed-in session) when a CLI command fails
    // because the server no longer accepts the session, e.g. an HTTP 401
    // "Invalid access token". The user has to sign in again.
    void sessionExpired();

private:
    // runCommand() timeout value that waits for the CLI indefinitely.
    static constexpr int CLI_NO_TIMEOUT = 0;

    VpnState    m_state         = VpnState::Unknown;
    // Whether the last login check or sign-in succeeded and no sign-out has
    // happened since. Gates sessionExpired(): a command that fails with "please
    // sign in" while nobody is signed in is expected, not an expired session.
    bool        m_signedIn      = false;
    AccountType m_accountType   = AccountType::Unknown;
    QString     m_connectedServer;       // last server string seen while Connected
    QString     m_lastConnectCountry;    // country arg last passed to connectVpn()
    QString     m_lastConnectCity;       // city    arg last passed to connectVpn()
    QString     m_lastConnectServer;     // server name last connected to by name, e.g. "US-NJ#203"
    QMap<QString, QString> m_countries;                      // see countries()
    QMap<QString, QList<QPair<QString, QString>>> m_cities;  // see cities()
    QProcess*       m_signinProcess  = nullptr;
    std::unique_ptr<SigninFlow> m_signinFlow; // the conversation with the running `protonvpn signin`
    int             m_signinAttempt  = 0;     // bumped when a sign-in ends, so late replies are ignored
    StatusMonitor*  m_statusMonitor  = nullptr;
    QMap<QString, QString> m_settings;   // see settings()
    int             m_configSetsInFlight = 0; // `config set` commands not yet finished

    // The settings saved in the CLI's settings.json, in fetchSettings() form.
    // Only what the user has changed is in there, so this is the fallback for
    // when the CLI cannot answer, and the full custom DNS list.
    static QMap<QString, QString> readSettingsFile();

    // Sign-in: the running flow's effects, and the `protonvpn signin` they drive.
    void applySigninEffects(const SigninEffects& effects);
    void startSigninProcess(const QStringList& arguments);
    // Interrupts the running sign-in CLI, if any, and ignores it from then on.
    void stopSigninProcess();
    // Drops the sign-in in progress: its CLI, its flow, and any reply still on its way.
    void endSignin();
    void finishSignin(bool ok, const QString& error);

    // Runs `protonvpn <args>` and calls back with its exit code and output.
    // With a timeoutMs, a CLI that has not exited by then is terminated and
    // reported with exit code CLI_EXIT_TIMED_OUT; one that cannot be started
    // at all is reported with CLI_EXIT_FAILED_TO_START (see protonvpnCli.cpp).
    void runCommand(const QStringList& args,
                    const std::function<void(int exitCode, const QString& output, const QString& errOutput)>& callback,
                    int timeoutMs = CLI_NO_TIMEOUT);

    void checkLoginStatus(int retriesLeft);

    // Polls `nmcli` for NetworkManager's general state, retrying with backoff
    // until it reports "connected*" or retriesLeft runs out. Calls onReady()
    // either way (fails open) so a missing nmcli / non-NM system never blocks
    // auto-connect indefinitely.
    void checkNetworkReady(int retriesLeft, const std::function<void()>& onReady);

    // Records where a connection is going and enters Connecting, then issues
    // it with no retries. A server name, when given, wins over country/city.
    void beginConnect(const QString& country, const QString& city, const QString& server);
    // The first half of that, shared with startupAutoConnect(): logs `action`
    // and the destination, records it, and enters Connecting.
    void enterConnecting(const QString& action, const QString& country, const QString& city,
                         const QString& server);

    // Runs `protonvpn connect`, retrying with backoff up to retriesLeft times
    // on failure before giving up and emitting VpnState::Error. retriesLeft=0
    // means no retry (used by the manual connectVpn() path). A non-empty
    // server connects to that server by name instead of country/city.
    void issueConnect(const QString& country, const QString& city, int retriesLeft,
                      const QString& server = QString());

    // Background status monitor (long-lived subprocess, every 15 s while logged in).
    void startStatusMonitor();
    void stopStatusMonitor();

    // Apply a parsed `protonvpn status` snapshot to internal state and emit
    // the appropriate signals.  Only emits when state or connected server
    // actually changed, to avoid unnecessary UI redraws.  Snapshots that could
    // not be parsed (no "status" field) are ignored rather than read as a
    // disconnect.
    void applyStatusFields(const QMap<QString, QString>& fields);

    // Human-readable name for a state, used in diagnostics.
    static QString stateToString(VpnState state);
};
