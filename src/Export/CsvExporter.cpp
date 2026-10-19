#include "CsvExporter.h"

#include <QFile>
#include <QLocale>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

namespace
{
QString quoted(QString s)
{
    s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + s + QLatin1Char('"');
}

bool openStream(QSaveFile& file, QString* error)
{
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool finish(QSaveFile& file, QString* error)
{
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QTextStream& prepare(QTextStream& out)
{
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);   // reconnu par Excel
    return out;
}
}

QString CsvExporter::number(double value, int decimals)
{
    if (!VehicleSample::valid(value))
        return QString();
    QString s = QString::number(value, 'f', decimals);
    s.replace(QLatin1Char('.'), QLatin1Char(','));
    return s;
}

bool CsvExporter::exportHistory(const QString& filename, const QList<VehicleSample>& samples, QString* error)
{
    QSaveFile file(filename);
    if (!openStream(file, error))
        return false;

    QTextStream out(&file);
    prepare(out);

    out << "Date;Heure;Régime (tr/min);Vitesse (km/h);Temp. moteur (°C);Temp. admission (°C);"
           "Papillon (%);Charge (%);Débit d'air (g/s);Pression carburant (kPa);Tension (V)\n";

    for (const VehicleSample& s : samples) {
        out << s.timestamp.toString(QStringLiteral("dd/MM/yyyy")) << ';'
            << s.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")) << ';'
            << number(s.rpm, 0) << ';'
            << number(s.speed, 0) << ';'
            << number(s.coolant, 0) << ';'
            << number(s.intakeTemp, 0) << ';'
            << number(s.throttle, 1) << ';'
            << number(s.engineLoad, 1) << ';'
            << number(s.maf, 2) << ';'
            << number(s.fuelPressure, 0) << ';'
            << number(s.batteryVoltage, 1) << '\n';
    }

    out.flush();
    return finish(file, error);
}

bool CsvExporter::exportDtcs(const QString& filename, const QList<DTC>& dtcs, QString* error)
{
    QSaveFile file(filename);
    if (!openStream(file, error))
        return false;

    QTextStream out(&file);
    prepare(out);

    out << "Code;Description;État;Gravité\n";
    for (const DTC& d : dtcs) {
        out << d.code << ';' << quoted(d.description) << ';'
            << DTC::statusText(d.status) << ';' << d.severity << '\n';
    }

    out.flush();
    return finish(file, error);
}

bool CsvExporter::exportTrips(const QString& filename, const QList<Trip>& trips, QString* error)
{
    QSaveFile file(filename);
    if (!openStream(file, error))
        return false;

    QTextStream out(&file);
    prepare(out);

    out << "N°;Début;Fin;Durée (min);Distance (km);Vitesse moyenne (km/h);Vitesse max (km/h);"
           "Régime moyen (tr/min);Carburant estimé (L)\n";
    for (const Trip& t : trips) {
        out << t.id << ';'
            << t.startTime.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")) << ';'
            << t.endTime.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")) << ';'
            << number(t.startTime.secsTo(t.endTime) / 60.0, 1) << ';'
            << number(t.distance, 2) << ';'
            << number(t.avgSpeed, 1) << ';'
            << number(t.maxSpeed, 0) << ';'
            << number(t.avgRpm, 0) << ';'
            << number(t.fuelUsed, 2) << '\n';
    }

    out.flush();
    return finish(file, error);
}
