#include "accountPage.h"

#include <QHBoxLayout>
#include <QSvgWidget>

#include "../uiHelpers.h"
#include "../widgets/styleUtils.h"

namespace
{
constexpr int ACCOUNT_PAGE_MARGIN  = 16;
constexpr int ACCOUNT_PAGE_SPACING = 12;
constexpr int INFO_ROW_H_MARGIN    = 16;
constexpr int INFO_ROW_V_MARGIN    = 10;
constexpr int REFRESH_BTN_HEIGHT   = 30;
constexpr int SPINNER_INTERVAL_MS  = 200;

QWidget* makeInfoRow(const QString& labelText, QLabel*& valueLabel, QWidget* parent)
{
    QWidget* row = new QWidget(parent);
    row->setObjectName(QStringLiteral("infoRow"));
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(INFO_ROW_H_MARGIN, INFO_ROW_V_MARGIN, INFO_ROW_H_MARGIN, INFO_ROW_V_MARGIN);
    QLabel* label = new QLabel(labelText, row);
    label->setObjectName(QStringLiteral("infoKey"));
    layout->addWidget(label);
    layout->addStretch();
    valueLabel = new QLabel(QStringLiteral("-"), row);
    valueLabel->setObjectName(QStringLiteral("infoValue"));
    layout->addWidget(valueLabel);
    return row;
}
} // namespace

AccountPage::AccountPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(ACCOUNT_PAGE_MARGIN, ACCOUNT_PAGE_MARGIN, ACCOUNT_PAGE_MARGIN, ACCOUNT_PAGE_MARGIN);
    layout->setSpacing(ACCOUNT_PAGE_SPACING);

    // Header
    QHBoxLayout* headerRow = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(tr("Account"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();
    m_refreshBtn = new QPushButton(tr("\u21bb Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(REFRESH_BTN_HEIGHT);
    connect(m_refreshBtn, &QPushButton::clicked, this, &AccountPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    layout->addLayout(headerRow);

    // Info card
    QWidget* card = new QWidget(this);
    card->setObjectName(QStringLiteral("infoCard"));
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    cardLayout->addWidget(makeInfoRow(tr("Username"), m_nameLabel, card));
    cardLayout->addWidget(makeInfoRow(tr("Plan"),     m_planLabel, card));

    layout->addWidget(card);

    // Upgrade prompt - shown only for Free accounts.
    m_upgradeLabel = new QLabel(
        tr("To upgrade to VPN Plus visit: "
           "<a href=\"https://account.protonvpn.com/pricing\">"
           "https://account.protonvpn.com/pricing</a>"),
        this);
    m_upgradeLabel->setTextFormat(Qt::RichText);
    m_upgradeLabel->setOpenExternalLinks(true);
    m_upgradeLabel->setWordWrap(true);
    m_upgradeLabel->setObjectName(QStringLiteral("upgradeLabel"));
    m_upgradeLabel->hide();
    layout->addWidget(m_upgradeLabel);

    layout->addStretch();

    // Sign out button
    QPushButton* signOutBtn = new QPushButton(tr("Sign Out"), this);
    signOutBtn->setObjectName(QStringLiteral("dangerButton"));
    signOutBtn->setCursor(Qt::PointingHandCursor);
    connect(signOutBtn, &QPushButton::clicked, this, &AccountPage::signOutRequested);
    layout->addWidget(signOutBtn);

    connect(m_manager, &VpnManager::infoReady,        this, &AccountPage::onInfoReady);
    connect(m_manager, &VpnManager::accountTypeReady, this, &AccountPage::onAccountTypeReady);

    // Show the account type we already know (fetched at startup / login).
    if (m_manager->accountType() != AccountType::Unknown)
    {
        onAccountTypeReady(m_manager->accountType());
    }

    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(SPINNER_INTERVAL_MS);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % kSpinnerFrameCount;
        m_nameLabel->setText(
            tr("%1 Loading\u2026").arg(QString::fromUtf8(kSpinnerFrames[m_spinnerFrame])));
    });
}

void AccountPage::refresh()
{
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(tr("Loading\u2026"));
    m_spinnerFrame = 0;
    m_nameLabel->setText(tr("\u280b Loading\u2026"));
    m_spinnerTimer->start();
    m_manager->fetchInfo();
    m_manager->fetchSettings(); // also reports the plan
}

void AccountPage::onInfoReady(const QMap<QString, QString>& info) const
{
    m_spinnerTimer->stop();
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(tr("\u21bb Refresh"));

    auto get = [&](const QString& key) -> QString
    {
        const QString val = info.value(key, QStringLiteral("-"));
        return (val == QStringLiteral("None") || val.isEmpty()) ? QStringLiteral("-") : val;
    };

    m_nameLabel->setText(get(QStringLiteral("Account")));
}

void AccountPage::onAccountTypeReady(AccountType type) const
{
    switch (type)
    {
        case AccountType::Free:
            m_planLabel->setText(tr("Free"));
            setStyleProperty(m_planLabel, "plan", QStringLiteral("free"));
            m_upgradeLabel->show();
            break;

        case AccountType::Plus:
            m_planLabel->setText(tr("VPN Plus"));
            setStyleProperty(m_planLabel, "plan", QStringLiteral("plus"));
            m_upgradeLabel->hide();
            break;

        default:
            m_planLabel->setText(QStringLiteral("\u2014"));
            setStyleProperty(m_planLabel, "plan", QString());
            m_upgradeLabel->hide();
            break;
    }
}
