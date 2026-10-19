// Tests unitaires du parseur OBD-II et du décodage DTC (aucun matériel requis).
// Lancement : ctest   ou   ./tst_obdparser

#include "DTC/DTCManager.h"
#include "OBD/ObdParser.h"

#include <QtTest>

#include <cmath>

class TstObdParser : public QObject
{
    Q_OBJECT

private slots:
    void rpmWithSpaces()
    {
        QCOMPARE(ObdParser::parseRPM(QStringLiteral("41 0C 1A F8\r\r>")), 1726.0);
    }

    void rpmWithoutSpacesAndEcho()
    {
        const QString raw = QStringLiteral("010C\r410C1AF8\r");
        const auto data = ObdParser::pidData(raw, 0x0C);
        QVERIFY(data.has_value());
        QCOMPARE(*ObdParser::decodePid(0x0C, *data), 1726.0);
    }

    void searchingLineIgnored()
    {
        QCOMPARE(ObdParser::parseSpeed(QStringLiteral("SEARCHING...\r41 0D 32\r")), 50.0);
    }

    void coolantAndThrottle()
    {
        QCOMPARE(ObdParser::parseCoolant(QStringLiteral("41 05 7B")), 83.0);
        QCOMPARE(qRound(ObdParser::parseThrottle(QStringLiteral("41 11 FF"))), 100);
    }

    void noDataIsError()
    {
        QVERIFY(ObdParser::isError(QStringLiteral("NO DATA")));
        QVERIFY(ObdParser::isError(QStringLiteral("UNABLE TO CONNECT")));
        QVERIFY(!ObdParser::isError(QStringLiteral("41 0C 1A F8")));
        QVERIFY(std::isnan(ObdParser::parseRPM(QStringLiteral("NO DATA"))));
    }

    void vinCanMultiFrame()
    {
        const QString raw = QStringLiteral(
            "014\r"
            "0: 49 02 01 56 46 33\r"
            "1: 4C 43 59 48 5A 50 48\r"
            "2: 53 31 32 33 34 35 36\r");
        QCOMPARE(ObdParser::parseVIN(raw), QStringLiteral("VF3LCYHZPHS123456"));
    }

    void vinCanMultiFrameNoSpaces()
    {
        const QString raw = QStringLiteral("014\r0:490201564633\r1:4C4359485A5048\r2:53313233343536\r");
        QCOMPARE(ObdParser::parseVIN(raw), QStringLiteral("VF3LCYHZPHS123456"));
    }

    void vinLegacyKLine()
    {
        // ISO 9141 / KWP2000 : 5 lignes de 4 octets, la première avec 3 octets de bourrage.
        const QString raw = QStringLiteral(
            "49 02 01 00 00 00 56\r"
            "49 02 02 46 33 4C 43\r"
            "49 02 03 59 48 5A 50\r"
            "49 02 04 48 53 31 32\r"
            "49 02 05 33 34 35 36\r");
        QCOMPARE(ObdParser::parseVIN(raw), QStringLiteral("VF3LCYHZPHS123456"));
    }

    void supportedPids()
    {
        // BE 1F A8 13 : exemple classique de la norme SAE J1979.
        const QSet<int> pids = ObdParser::parseSupportedPids(QStringLiteral("41 00 BE 1F A8 13"), 0x00);
        QVERIFY(pids.contains(0x01));
        QVERIFY(pids.contains(0x0C));
        QVERIFY(pids.contains(0x0D));
        QVERIFY(!pids.contains(0x02));
        QVERIFY(pids.contains(0x20));
    }

    void voltage()
    {
        QCOMPARE(*ObdParser::parseVoltage(QStringLiteral("12.6V")), 12.6);
        QVERIFY(!ObdParser::parseVoltage(QStringLiteral("?")).has_value());
    }

    void protocol()
    {
        QVERIFY(ObdParser::isCanProtocol(QStringLiteral("A6")));
        QVERIFY(ObdParser::isCanProtocol(QStringLiteral("8")));
        QVERIFY(!ObdParser::isCanProtocol(QStringLiteral("A3")));
    }

    void dtcCodeConversion()
    {
        QCOMPARE(DTCManager::bytesToCode(0x01, 0x33), QStringLiteral("P0133"));
        QCOMPARE(DTCManager::bytesToCode(0x04, 0x20), QStringLiteral("P0420"));
        QCOMPARE(DTCManager::bytesToCode(0xC1, 0x00), QStringLiteral("U0100"));
        QCOMPARE(DTCManager::bytesToCode(0x42, 0x2F), QStringLiteral("C022F"));
        QCOMPARE(DTCManager::bytesToCode(0x92, 0x34), QStringLiteral("B1234"));
    }

    void dtcCanResponse()
    {
        const QList<DTC> list = DTCManager::parseResponse(QStringLiteral("43 02 01 33 04 20"));
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).code, QStringLiteral("P0133"));
        QCOMPARE(list.at(1).code, QStringLiteral("P0420"));
    }

    void dtcLegacyResponse()
    {
        const QList<DTC> list = DTCManager::parseResponse(QStringLiteral("43 01 33 04 20 00 00\r"));
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(1).code, QStringLiteral("P0420"));
    }

    void dtcNone()
    {
        QVERIFY(DTCManager::parseResponse(QStringLiteral("43 00")).isEmpty());
        QVERIFY(DTCManager::parseResponse(QStringLiteral("NO DATA")).isEmpty());
    }

    void dtcDescription()
    {
        QVERIFY(DTCManager::describe(QStringLiteral("P0420")).contains(QStringLiteral("catalyseur")));
        QVERIFY(!DTCManager::describe(QStringLiteral("P1ABC")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstObdParser)
#include "tst_obdparser.moc"
