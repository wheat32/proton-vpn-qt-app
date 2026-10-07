#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

// "Available to Plus Members" between two lines, shown to free accounts above
// the settings only Plus members can change.
class PlusDivider : public QWidget
{
    static constexpr int H_MARGIN = 16;
    static constexpr int V_MARGIN = 10;
    static constexpr int SPACING  = 10;

    Q_OBJECT

public:
    explicit PlusDivider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        QHBoxLayout* hl = new QHBoxLayout(this);
        hl->setContentsMargins(H_MARGIN, V_MARGIN, H_MARGIN, V_MARGIN);
        hl->setSpacing(SPACING);

        hl->addWidget(makeLine(), 1);

        QLabel* label = new QLabel(tr("✦  Available to Plus Members"), this);
        label->setObjectName(QStringLiteral("plusDividerLabel"));
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        hl->addWidget(label);

        hl->addWidget(makeLine(), 1);
    }

private:
    QFrame* makeLine()
    {
        QFrame* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setObjectName(QStringLiteral("plusDividerLine"));
        return line;
    }
};
