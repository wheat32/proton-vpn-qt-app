#include <QtTest/QtTest>
#include "widgets/numberSpinner.h"

// NumberSpinner::valueFromText() decides what the spinner holds once the user
// is done typing. The field once kept showing a number it had not accepted
// (out of range, or nothing at all) while the setting stayed what it was.

class TstNumberSpinner : public QObject
{
    Q_OBJECT

private:
    static constexpr int MIN = 0;
    static constexpr int MAX = 20;
    static constexpr int CURRENT = 5;

private slots:
    void valueFromText_numberInRange_isAccepted()
    {
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("7"), MIN, MAX, CURRENT), 7);
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral(" 12 "), MIN, MAX, CURRENT), 12);
    }

    void valueFromText_rangeLimits_areAccepted()
    {
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("0"), MIN, MAX, CURRENT), MIN);
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("20"), MIN, MAX, CURRENT), MAX);
    }

    void valueFromText_outOfRange_keepsCurrentValue()
    {
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("25"), MIN, MAX, CURRENT), CURRENT);
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("99"), MIN, MAX, CURRENT), CURRENT);
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("-1"), MIN, MAX, CURRENT), CURRENT);
    }

    void valueFromText_emptyOrNotANumber_keepsCurrentValue()
    {
        QCOMPARE(NumberSpinner::valueFromText(QString(), MIN, MAX, CURRENT), CURRENT);
        QCOMPARE(NumberSpinner::valueFromText(QStringLiteral("abc"), MIN, MAX, CURRENT), CURRENT);
    }
};

QTEST_MAIN(TstNumberSpinner)
#include "tst_numberSpinner.moc"
