#pragma once

#include <QString>

// Commandes AT (ELM327 / STN11xx) et requêtes OBD-II standard SAE J1979.
namespace ObdCommand
{
// --- Configuration de l'adaptateur ----------------------------------------
inline QString Reset()          { return QStringLiteral("ATZ"); }
inline QString Defaults()       { return QStringLiteral("ATD"); }
inline QString EchoOff()        { return QStringLiteral("ATE0"); }
inline QString LineFeedOff()    { return QStringLiteral("ATL0"); }
inline QString SpacesOff()      { return QStringLiteral("ATS0"); }
inline QString HeadersOff()     { return QStringLiteral("ATH0"); }
inline QString AdaptiveTiming() { return QStringLiteral("ATAT1"); }
inline QString AutoProtocol()   { return QStringLiteral("ATSP0"); }
inline QString Identify()       { return QStringLiteral("ATI"); }
inline QString StnIdentify()    { return QStringLiteral("STI"); }
inline QString ProtocolNumber() { return QStringLiteral("ATDPN"); }
inline QString ProtocolName()   { return QStringLiteral("ATDP"); }
inline QString BatteryVoltage() { return QStringLiteral("ATRV"); }

// --- Mode 01 : données temps réel ------------------------------------------
inline QString SupportedPids()  { return QStringLiteral("0100"); }
inline QString MonitorStatus()  { return QStringLiteral("0101"); }
inline QString EngineLoad()     { return QStringLiteral("0104"); }
inline QString Coolant()        { return QStringLiteral("0105"); }
inline QString FuelPressure()   { return QStringLiteral("010A"); }
inline QString RPM()            { return QStringLiteral("010C"); }
inline QString Speed()          { return QStringLiteral("010D"); }
inline QString IntakeTemp()     { return QStringLiteral("010F"); }
inline QString MAF()            { return QStringLiteral("0110"); }
inline QString Throttle()       { return QStringLiteral("0111"); }

inline QString Mode01(int pid)
{
    return QStringLiteral("01%1").arg(pid, 2, 16, QLatin1Char('0')).toUpper();
}

// --- Défauts ----------------------------------------------------------------
inline QString ReadDTC()        { return QStringLiteral("03"); }
inline QString ClearDTC()       { return QStringLiteral("04"); }
inline QString PendingDTC()     { return QStringLiteral("07"); }
inline QString PermanentDTC()   { return QStringLiteral("0A"); }

// --- Mode 09 : informations véhicule ----------------------------------------
inline QString ReadVIN()        { return QStringLiteral("0902"); }
}

// Identifiants des PIDs mode 01 utilisés par l'application.
namespace Pid
{
constexpr int MonitorStatus = 0x01;
constexpr int EngineLoad    = 0x04;
constexpr int Coolant       = 0x05;
constexpr int FuelPressure  = 0x0A;
constexpr int RPM           = 0x0C;
constexpr int Speed         = 0x0D;
constexpr int IntakeTemp    = 0x0F;
constexpr int MAF           = 0x10;
constexpr int Throttle      = 0x11;
}
