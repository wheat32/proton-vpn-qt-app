#include "loginPage.h"
#include "../appVersion.h"
#include "../geoUtils.h"
#include "../widgets/stackPage.h"
#include "../widgets/svgBanner.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVersionNumber>

namespace
{
constexpr int LOGIN_CARD_WIDTH          = 360;
constexpr int BANNER_AREA_MAX_HEIGHT    = 150;
constexpr int LOGIN_LAYOUT_SPACING      = 16;
constexpr int LOGIN_H_MARGIN            = 32;
constexpr int LOGIN_TOP_MARGIN          = 32;
constexpr int LOGIN_BOT_MARGIN          = 24;
constexpr int LOGIN_LOGO_SPACING        = 8;
constexpr int LOGIN_FIELD_SPACING       = 4;
constexpr double LOGIN_LOGO_SCALE       = 4.0;
constexpr int PASSWORD_TOGGLE_ICON_SIZE = 20;
constexpr int PASSWORD_TOGGLE_BTN_SIZE  = 36;
constexpr int SVG_ICON_SIZE             = 20;
constexpr int TFA_MAX_LENGTH            = 8;
constexpr int ERROR_SCROLL_HEIGHT       = 100;
constexpr int ERROR_DETAILS_BTN_WIDTH   = 140;
constexpr int ERROR_CONTAINER_SPACING   = 6;
constexpr int ERROR_H_MARGIN            = 32;
constexpr int ERROR_BOT_MARGIN          = 16;
constexpr int STACK_INDEX_CREDS         = 0;
constexpr int STACK_INDEX_TFA           = 1;
constexpr int STACK_INDEX_KEY           = 2;
// security-key.svg (a key going into a laptop) is drawn 329 x 120.
constexpr double SECURITY_KEY_ART_WIDTH  = 329.0;
constexpr double SECURITY_KEY_ART_HEIGHT = 120.0;

QIcon svgIcon(const QString& path, const QSize& size = {SVG_ICON_SIZE, SVG_ICON_SIZE})
{
    return QIcon(GeoUtils::svgPixmap(path, size.width(), size.height()));
}
} // namespace

LoginPage::LoginPage(QWidget* parent)
    : QWidget(parent)
{
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setAlignment(Qt::AlignCenter);

    QWidget* card = new QWidget(this);
    card->setObjectName(QStringLiteral("loginCard"));
    card->setFixedWidth(LOGIN_CARD_WIDTH);

    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(0);
    cardLayout->setContentsMargins(0, 0, 0, 0);

    // Inner stack: 0 = credentials, 1 = 2FA code, 2 = security key. Each is
    // a StackPage, so it keeps its own height rather than the tallest one's.
    m_stack = new QStackedWidget(card);

    buildCredsWidget();
    buildTFAWidget();
    buildSecurityKeyWidget();

    m_stack->addWidget(m_credsWidget); // index 0
    m_stack->addWidget(m_tfaWidget);   // index 1
    m_stack->addWidget(m_keyWidget);   // index 2
    cardLayout->addWidget(m_stack);

    // Shared error section below the stack
    m_errorContainer = new QWidget(card);
    m_errorContainer->setVisible(false);
    QVBoxLayout* errorContainerLayout = new QVBoxLayout(m_errorContainer);
    errorContainerLayout->setSpacing(ERROR_CONTAINER_SPACING);
    errorContainerLayout->setContentsMargins(ERROR_H_MARGIN, 0, ERROR_H_MARGIN, ERROR_BOT_MARGIN);

    // Scrollable error label
    m_errorLabel = new QLabel();
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_errorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_errorScrollArea = new QScrollArea(m_errorContainer);
    m_errorScrollArea->setWidget(m_errorLabel);
    m_errorScrollArea->setWidgetResizable(true);
    m_errorScrollArea->setMaximumHeight(ERROR_SCROLL_HEIGHT);
    m_errorScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_errorScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_errorScrollArea->setObjectName(QStringLiteral("errorLabel"));
    m_errorScrollArea->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    errorContainerLayout->addWidget(m_errorScrollArea);

    // "View Details" button
    m_errorDetailsBtn = new QPushButton(tr("View Details"), m_errorContainer);
    m_errorDetailsBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_errorDetailsBtn->setCursor(Qt::PointingHandCursor);
    m_errorDetailsBtn->setFixedWidth(ERROR_DETAILS_BTN_WIDTH);
    connect(m_errorDetailsBtn, &QPushButton::clicked, this, [this]()
    {
        ErrorDetailsDialog* dlg = new ErrorDetailsDialog(m_rawError, this);
        dlg->exec();
    });
    errorContainerLayout->addWidget(m_errorDetailsBtn, 0, Qt::AlignCenter);

    cardLayout->addWidget(m_errorContainer);

    m_outerLayout->addWidget(card, 0, Qt::AlignCenter);

    // Banner scroll area - holds warning banners below the card.
    // Capped at BANNER_AREA_MAX_HEIGHT so banners can never squish the
    // login/2FA input fields when multiple warnings are visible at once.
    QWidget* bannerContainer = new QWidget(this);
    m_bannerLayout = new QVBoxLayout(bannerContainer);
    m_bannerLayout->setContentsMargins(0, 0, 0, 0);
    m_bannerLayout->setSpacing(0);

    m_bannerScrollArea = new QScrollArea(this);
    m_bannerScrollArea->setWidget(bannerContainer);
    m_bannerScrollArea->setWidgetResizable(true);
    m_bannerScrollArea->setFrameShape(QFrame::NoFrame);
    m_bannerScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bannerScrollArea->setMaximumHeight(BANNER_AREA_MAX_HEIGHT);
    m_bannerScrollArea->setVisible(false);
    m_outerLayout->addWidget(m_bannerScrollArea);

    // Show a banner if this is a pre-release build
    checkPrereleaseBanner();
}

void LoginPage::buildCredsWidget()
{
    m_credsWidget = new StackPage();
    QVBoxLayout* layout = new QVBoxLayout(m_credsWidget);
    layout->setSpacing(LOGIN_LAYOUT_SPACING);
    layout->setContentsMargins(LOGIN_H_MARGIN, LOGIN_TOP_MARGIN, LOGIN_H_MARGIN, LOGIN_BOT_MARGIN);

    // Logo
    SvgBanner* logo = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), LOGIN_LOGO_SCALE, m_credsWidget);
    logo->setLightResource(QStringLiteral(":/assets/proton-vpn-logo-light.svg"));
    layout->addWidget(logo, 0, Qt::AlignCenter);

    layout->addSpacing(LOGIN_LOGO_SPACING);

    QLabel* titleLabel = new QLabel(tr("Sign in to Proton VPN"), m_credsWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    // Username
    QLabel* usernameLabel = new QLabel(tr("Username"), m_credsWidget);
    usernameLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(usernameLabel);

    m_usernameEdit = new QLineEdit(m_credsWidget);
    m_usernameEdit->setObjectName(QStringLiteral("inputField"));
    m_usernameEdit->setPlaceholderText(tr("Enter your username"));
    layout->addWidget(m_usernameEdit);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    // Password
    QLabel* passwordLabel = new QLabel(tr("Password"), m_credsWidget);
    passwordLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(passwordLabel);

    QHBoxLayout* passwordRow = new QHBoxLayout();
    m_passwordEdit = new QLineEdit(m_credsWidget);
    m_passwordEdit->setObjectName(QStringLiteral("inputField"));
    m_passwordEdit->setPlaceholderText(tr("Enter your password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passwordRow->addWidget(m_passwordEdit);

    m_togglePasswordBtn = new QPushButton(m_credsWidget);
    m_togglePasswordBtn->setObjectName(QStringLiteral("iconButton"));
    m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-show.svg")));
    m_togglePasswordBtn->setIconSize({PASSWORD_TOGGLE_ICON_SIZE, PASSWORD_TOGGLE_ICON_SIZE});
    m_togglePasswordBtn->setFixedSize(PASSWORD_TOGGLE_BTN_SIZE, PASSWORD_TOGGLE_BTN_SIZE);
    m_togglePasswordBtn->setCheckable(true);
    m_togglePasswordBtn->setToolTip(tr("Show/hide password"));
    m_togglePasswordBtn->setCursor(Qt::PointingHandCursor);
    connect(m_togglePasswordBtn, &QPushButton::toggled, this, [this](const bool checked)
    {
        m_passwordVisible = checked;
        togglePasswordVisibility();
    });
    passwordRow->addWidget(m_togglePasswordBtn);
    layout->addLayout(passwordRow);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    // Sign In button
    m_loginBtn = new QPushButton(tr("Sign In"), m_credsWidget);
    m_loginBtn->setObjectName(QStringLiteral("primaryButton"));
    m_loginBtn->setCursor(Qt::PointingHandCursor);
    connect(m_loginBtn, &QPushButton::clicked, this, [this]()
    {
        emit loginRequested(m_usernameEdit->text().trimmed(), m_passwordEdit->text());
    });
    connect(m_passwordEdit, &QLineEdit::returnPressed, m_loginBtn, &QPushButton::click);
    connect(m_usernameEdit, &QLineEdit::returnPressed, m_passwordEdit, [this]()
    {
        m_passwordEdit->setFocus();
    });

    // Both fields are required; submitting an empty form only produced a CLI
    // error round-trip.
    connect(m_usernameEdit, &QLineEdit::textChanged, this, &LoginPage::updateSignInEnabled);
    connect(m_passwordEdit, &QLineEdit::textChanged, this, &LoginPage::updateSignInEnabled);
    updateSignInEnabled();

    layout->addWidget(m_loginBtn);
}

void LoginPage::buildTFAWidget()
{
    m_tfaWidget = new StackPage();
    QVBoxLayout* layout = new QVBoxLayout(m_tfaWidget);
    layout->setSpacing(LOGIN_LAYOUT_SPACING);
    layout->setContentsMargins(LOGIN_H_MARGIN, LOGIN_TOP_MARGIN, LOGIN_H_MARGIN, LOGIN_BOT_MARGIN);

    // Logo
    SvgBanner* logo = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), LOGIN_LOGO_SCALE, m_tfaWidget);
    logo->setLightResource(QStringLiteral(":/assets/proton-vpn-logo-light.svg"));
    layout->addWidget(logo, 0, Qt::AlignCenter);

    layout->addSpacing(LOGIN_LOGO_SPACING);

    QLabel* titleLabel = new QLabel(tr("Two-Factor Authentication"), m_tfaWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QLabel* descLabel = new QLabel(
        tr("Enter the 6-digit code from your authenticator app."),
        m_tfaWidget);
    descLabel->setObjectName(QStringLiteral("fieldLabel"));
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    layout->addSpacing(LOGIN_FIELD_SPACING);

    QLabel* tfaLabel = new QLabel(tr("2FA Token"), m_tfaWidget);
    tfaLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(tfaLabel);

    m_tfaEdit = new QLineEdit(m_tfaWidget);
    m_tfaEdit->setObjectName(QStringLiteral("inputField"));
    m_tfaEdit->setPlaceholderText(tr("e.g. 123456"));
    m_tfaEdit->setMaxLength(TFA_MAX_LENGTH); // TOTP codes are 6 digits; allow 8 for recovery codes
    layout->addWidget(m_tfaEdit);

    m_tfaSubmitBtn = new QPushButton(tr("Verify"), m_tfaWidget);
    m_tfaSubmitBtn->setObjectName(QStringLiteral("primaryButton"));
    m_tfaSubmitBtn->setCursor(Qt::PointingHandCursor);
    connect(m_tfaSubmitBtn, &QPushButton::clicked, this, [this]()
    {
        emit twoFASubmitted(m_tfaEdit->text().trimmed());
    });
    connect(m_tfaEdit, &QLineEdit::returnPressed, m_tfaSubmitBtn, &QPushButton::click);
    layout->addWidget(m_tfaSubmitBtn);

    m_tfaCancelBtn = new QPushButton(tr("Go Back"), m_tfaWidget);
    m_tfaCancelBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_tfaCancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_tfaCancelBtn, &QPushButton::clicked, this, [this]()
    {
        emit loginCancelRequested();
    });
    layout->addWidget(m_tfaCancelBtn);
}

void LoginPage::buildSecurityKeyWidget()
{
    m_keyWidget = new StackPage();
    QVBoxLayout* layout = new QVBoxLayout(m_keyWidget);
    layout->setSpacing(LOGIN_LAYOUT_SPACING);
    layout->setContentsMargins(LOGIN_H_MARGIN, LOGIN_TOP_MARGIN, LOGIN_H_MARGIN, LOGIN_BOT_MARGIN);

    // A key going into a laptop, in place of the logo. Its laptop and arrow
    // are white for dark themes; the light version draws them dark.
    SvgBanner* art = new SvgBanner(QStringLiteral(":/assets/security-key.svg"),
                                   SECURITY_KEY_ART_WIDTH / SECURITY_KEY_ART_HEIGHT, m_keyWidget);
    art->setLightResource(QStringLiteral(":/assets/security-key-light.svg"));
    layout->addWidget(art, 0, Qt::AlignCenter);

    QLabel* titleLabel = new QLabel(tr("Security Key"), m_keyWidget);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    m_keyStatusLabel = new QLabel(m_keyWidget);
    m_keyStatusLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_keyStatusLabel->setWordWrap(true);
    m_keyStatusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_keyStatusLabel);

    m_keyPinLabel = new QLabel(tr("PIN"), m_keyWidget);
    m_keyPinLabel->setObjectName(QStringLiteral("fieldLabel"));
    layout->addWidget(m_keyPinLabel);

    m_keyPinEdit = new QLineEdit(m_keyWidget);
    m_keyPinEdit->setObjectName(QStringLiteral("inputField"));
    m_keyPinEdit->setPlaceholderText(tr("Security key PIN"));
    m_keyPinEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_keyPinEdit);

    m_keyActionBtn = new QPushButton(m_keyWidget);
    m_keyActionBtn->setObjectName(QStringLiteral("primaryButton"));
    m_keyActionBtn->setCursor(Qt::PointingHandCursor);
    connect(m_keyActionBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_keyPrompt == SigninPrompt::SecurityKeyPin)
        {
            emit securityKeyPinSubmitted(m_keyPinEdit->text());
        }
        else
        {
            emit securityKeyRetryRequested();
        }
    });
    connect(m_keyPinEdit, &QLineEdit::returnPressed, m_keyActionBtn, &QPushButton::click);
    layout->addWidget(m_keyActionBtn);

    m_keyUseCodeBtn = new QPushButton(tr("Use Authenticator Code Instead"), m_keyWidget);
    m_keyUseCodeBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_keyUseCodeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_keyUseCodeBtn, &QPushButton::clicked, this, &LoginPage::useAuthenticatorCodeRequested);
    layout->addWidget(m_keyUseCodeBtn);

    m_keyCancelBtn = new QPushButton(tr("Go Back"), m_keyWidget);
    m_keyCancelBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_keyCancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_keyCancelBtn, &QPushButton::clicked, this, &LoginPage::loginCancelRequested);
    layout->addWidget(m_keyCancelBtn);
}

void LoginPage::showSigninPrompt(const SigninPrompt prompt, const QString& error)
{
    if (prompt == SigninPrompt::Code)
    {
        showCodePrompt();
    }
    else
    {
        showSecurityKey(prompt);
    }
    setError(error);
}

void LoginPage::showSecurityKey(const SigninPrompt prompt)
{
    m_keyPrompt = prompt;

    QString status;
    QString action;
    switch (prompt)
    {
        case SigninPrompt::SecurityKeyTouch:
            status = tr("Press the button on your security key.");
            break;

        case SigninPrompt::SecurityKeyPin:
            status = tr("Enter the PIN for your security key.");
            action = tr("Continue");
            break;

        case SigninPrompt::SecurityKeyMissing:
            status = tr("No security key detected. Insert your security key, then select Try Again.");
            action = tr("Try Again");
            break;

        case SigninPrompt::SecurityKeyChoose:
            status = tr("Multiple security keys were found. Tap the one you want to use.");
            break;

        case SigninPrompt::SecurityKeyRead:
            status = tr("Security key read. Signing in\u2026");
            break;

        case SigninPrompt::SecurityKeyFailed:
            status = tr("Your security key could not be used to sign in.");
            action = tr("Try Again");
            break;

        case SigninPrompt::SecurityKey:
        case SigninPrompt::Code:
        default:
            status = tr("Waiting for your security key\u2026");
            break;
    }

    const bool askingPin = prompt == SigninPrompt::SecurityKeyPin;
    m_keyStatusLabel->setText(status);
    m_keyPinLabel->setVisible(askingPin);
    m_keyPinEdit->setVisible(askingPin);
    m_keyPinEdit->clear();
    m_keyPinEdit->setEnabled(true);
    m_keyActionBtn->setVisible(action.isEmpty() == false);
    m_keyActionBtn->setText(action);
    m_keyActionBtn->setEnabled(true);
    // Once the key has been read, signing in is already under way: there is
    // nothing to switch to or go back from.
    const bool keyRead = prompt == SigninPrompt::SecurityKeyRead;
    m_keyUseCodeBtn->setVisible(keyRead == false);
    m_keyUseCodeBtn->setEnabled(true);
    m_keyCancelBtn->setVisible(keyRead == false);
    m_keyCancelBtn->setText(tr("Go Back")); // a Debug page preview relabels it
    m_keyCancelBtn->setEnabled(true);

    m_stack->setCurrentIndex(STACK_INDEX_KEY);
    if (askingPin == true)
    {
        m_keyPinEdit->setFocus();
    }
}

#ifdef QT_DEBUG
void LoginPage::showSigninPreview(const SigninPrompt prompt, const QString& error)
{
    showSigninPrompt(prompt, error);
    // The signing-in screen has no Go Back, but a preview needs one to
    // return to the Debug page.
    if (prompt == SigninPrompt::SecurityKeyRead)
    {
        m_keyCancelBtn->setText(tr("DEBUG: Go Back"));
        m_keyCancelBtn->setVisible(true);
    }
}
#endif

void LoginPage::showCodePrompt() const
{
    setError(QString());
    m_tfaEdit->clear();
    // Everything setLoading() disables while a code is checked. A rejected
    // code comes back as this prompt, not as loginFinished, so nothing else
    // would enable Go Back again.
    m_tfaSubmitBtn->setEnabled(true);
    m_tfaSubmitBtn->setText(tr("Verify"));
    m_tfaEdit->setEnabled(true);
    m_tfaCancelBtn->setEnabled(true);
    m_stack->setCurrentIndex(STACK_INDEX_TFA);
    m_tfaEdit->setFocus();
}

void LoginPage::reset() const
{
    m_stack->setCurrentIndex(STACK_INDEX_CREDS);
    setError(QString());
    m_passwordEdit->clear();
    m_tfaEdit->clear();
    m_keyPinEdit->clear();
    m_loginBtn->setText(tr("Sign In"));
    updateSignInEnabled();
    m_usernameEdit->setEnabled(true);
    m_passwordEdit->setEnabled(true);
    m_togglePasswordBtn->setEnabled(true);
}

void LoginPage::updateSignInEnabled() const
{
    m_loginBtn->setEnabled(m_usernameEdit->text().trimmed().isEmpty() == false
                           && m_passwordEdit->text().isEmpty() == false);
}

void LoginPage::setError(const QString& error) const
{
    if (error.isEmpty())
    {
        m_errorContainer->setVisible(false);
        m_errorLabel->clear();
        m_rawError = QString();
    }
    else
    {
        m_rawError = error;
        m_errorLabel->setText(error);

        // Size the scroll area to the label's natural wrapped height, capped at
        // ERROR_SCROLL_HEIGHT (where a scrollbar kicks in for very long errors).
        const int vpWidth = m_errorScrollArea->viewport()->width();
        const int labelH  = (vpWidth > 0)
            ? m_errorLabel->heightForWidth(vpWidth)
            : m_errorLabel->sizeHint().height();
        const int frameH  = m_errorScrollArea->frameWidth() * 2;
        m_errorScrollArea->setFixedHeight(qMin(labelH + frameH, ERROR_SCROLL_HEIGHT));

        m_errorContainer->setVisible(true);
    }
}

void LoginPage::setLoading(const bool loading) const
{
    if (m_stack->currentIndex() == STACK_INDEX_CREDS)
    {
        if (loading == true)
        {
            m_loginBtn->setEnabled(false);
        }
        else
        {
            updateSignInEnabled();
        }
        m_loginBtn->setText(loading == true ? tr("Signing in\u2026") : tr("Sign In"));
        m_usernameEdit->setEnabled(loading == false);
        m_passwordEdit->setEnabled(loading == false);
        m_togglePasswordBtn->setEnabled(loading == false);
    }
    else if (m_stack->currentIndex() == STACK_INDEX_TFA)
    {
        m_tfaSubmitBtn->setEnabled(loading == false);
        m_tfaSubmitBtn->setText(loading == true ? tr("Verifying\u2026") : tr("Verify"));
        m_tfaEdit->setEnabled(loading == false);
        m_tfaCancelBtn->setEnabled(loading == false);
    }
    else
    {
        // The CLI answers with the next prompt, which resets the view; Go
        // Back stays available meanwhile.
        m_keyActionBtn->setEnabled(loading == false);
        m_keyPinEdit->setEnabled(loading == false);
        m_keyUseCodeBtn->setEnabled(loading == false);
        if (m_keyPrompt == SigninPrompt::SecurityKeyPin)
        {
            m_keyActionBtn->setText(loading == true ? tr("Verifying\u2026") : tr("Continue"));
        }
    }
}

void LoginPage::togglePasswordVisibility() const
{
    if (m_passwordVisible == true)
    {
        m_passwordEdit->setEchoMode(QLineEdit::Normal);
        m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-hide.svg")));
    }
    else
    {
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_togglePasswordBtn->setIcon(svgIcon(QStringLiteral(":/assets/eye-show.svg")));
    }
}

void LoginPage::checkPrereleaseBanner()
{
    const AppVersion::Info& version = AppVersion::embedded();
    if (version.prerelease == false) return;

    const QString msg = tr(
        "You are running a <b>pre-release</b> version of this app (<b>v%1</b>). "
        "It may contain bugs or incomplete features. Use with caution.")
        .arg(version.appVersion.toHtmlEscaped());

    m_prereleaseBanner = new InfoBanner(msg, this);
    connect(m_prereleaseBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_prereleaseBanner = nullptr;
    });
    m_bannerLayout->addWidget(m_prereleaseBanner);
    m_bannerScrollArea->setVisible(true);
}

void LoginPage::onCliVersionReady(const QString& version)
{
    const QString cliVersionMin = AppVersion::embedded().cliTestedMin;
    const QString cliVersionMax = AppVersion::embedded().cliTestedMax;

    if (version.isEmpty() || (cliVersionMin.isEmpty() && cliVersionMax.isEmpty())) return;

    const QVersionNumber installed = QVersionNumber::fromString(version);
    const QVersionNumber verMin    = QVersionNumber::fromString(cliVersionMin);
    const QVersionNumber verMax    = QVersionNumber::fromString(cliVersionMax);

    const bool tooOld = installed.isNull() == false && verMin.isNull() == false
                        && installed < verMin;
    const bool tooNew = installed.isNull() == false && verMax.isNull() == false
                        && installed > verMax;

    if (tooOld == false && tooNew == false) return;

    const QString rangeStr = (cliVersionMin.isEmpty() == false && cliVersionMax.isEmpty() == false)
        ? (cliVersionMin + QStringLiteral("-") + cliVersionMax)
        : (cliVersionMin.isEmpty() ? cliVersionMax : cliVersionMin);

    static const QString WORKAROUND_MSG = tr(
        " If you cannot log in due to this incompatibility, open a terminal and run "
        "<code>protonvpn connect</code> as a workaround until an update is released.");

    const QString msg = tooNew == true
        ? tr("Your Proton VPN CLI (<b>v%1</b>) is newer than the tested range (<b>%2</b>). "
             "Things may work fine, but you could encounter unexpected behavior.%3")
              .arg(version, rangeStr, WORKAROUND_MSG)
        : tr("Your Proton VPN CLI (<b>v%1</b>) is older than the tested range (<b>%2</b>). "
             "Some features may not work correctly. Consider upgrading the CLI.%3")
              .arg(version, rangeStr, WORKAROUND_MSG);

    m_versionBanner = new InfoBanner(msg, this);
    connect(m_versionBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_versionBanner = nullptr;
    });
    m_outerLayout->addWidget(m_versionBanner);
}
