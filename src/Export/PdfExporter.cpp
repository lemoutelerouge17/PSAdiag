#include "PdfExporter.h"

#include "Core/AppInfo.h"
#include "CsvExporter.h"
#include "Utils/StatisticsManager.h"

#include <QFont>
#include <QFontMetrics>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>

namespace
{
class PdfPage
{
public:
    PdfPage(QPdfWriter& writer, QPainter& painter)
        : m_writer(writer), m_p(painter)
    {
        m_width = m_p.viewport().width();
        m_height = m_p.viewport().height();
    }

    int width() const { return m_width; }
    int y = 0;

    void ensure(int needed)
    {
        if (y + needed > m_height - footerHeight()) {
            footer();
            m_writer.newPage();
            ++m_page;
            y = 0;
        }
    }

    void title(const QString& text)
    {
        m_p.setFont(QFont(QStringLiteral("Arial"), 18, QFont::Bold));
        const int h = QFontMetrics(m_p.font(), &m_writer).height();
        ensure(h);
        m_p.setPen(QColor(25, 60, 120));
        m_p.drawText(QRect(0, y, m_width, h), Qt::AlignLeft | Qt::AlignVCenter, text);
        y += h + h / 3;
        m_p.setPen(QColor(25, 60, 120));
        m_p.drawLine(0, y, m_width, y);
        y += h / 2;
        m_p.setPen(Qt::black);
    }

    void section(const QString& text)
    {
        m_p.setFont(QFont(QStringLiteral("Arial"), 13, QFont::Bold));
        const int h = QFontMetrics(m_p.font(), &m_writer).height();
        ensure(h * 3);
        y += h / 2;
        m_p.setPen(QColor(25, 60, 120));
        m_p.drawText(QRect(0, y, m_width, h), Qt::AlignLeft | Qt::AlignVCenter, text);
        y += h + h / 4;
        m_p.setPen(Qt::black);
    }

    void line(const QString& label, const QString& value)
    {
        m_p.setFont(QFont(QStringLiteral("Arial"), 10));
        const int h = QFontMetrics(m_p.font(), &m_writer).height();
        ensure(h);
        m_p.drawText(QRect(0, y, m_width / 3, h), Qt::AlignLeft | Qt::AlignVCenter, label);
        m_p.drawText(QRect(m_width / 3, y, 2 * m_width / 3, h), Qt::AlignLeft | Qt::AlignVCenter, value);
        y += h + h / 6;
    }

    void text(const QString& value)
    {
        m_p.setFont(QFont(QStringLiteral("Arial"), 10));
        const QRect bounds = m_p.boundingRect(QRect(0, 0, m_width, 100000), Qt::TextWordWrap, value);
        ensure(bounds.height());
        m_p.drawText(QRect(0, y, m_width, bounds.height()), Qt::TextWordWrap, value);
        y += bounds.height() + bounds.height() / 6;
    }

    // Tableau simple ; widths en fractions de la largeur.
    void table(const QStringList& headers, const QList<QStringList>& rows, const QList<double>& widths)
    {
        m_p.setFont(QFont(QStringLiteral("Arial"), 9));
        const int h = QFontMetrics(m_p.font(), &m_writer).height() * 14 / 10;

        auto drawRow = [&](const QStringList& cells, bool header, bool shaded) {
            ensure(h);
            if (header)
                m_p.fillRect(QRect(0, y, m_width, h), QColor(25, 60, 120));
            else if (shaded)
                m_p.fillRect(QRect(0, y, m_width, h), QColor(238, 242, 248));

            QFont f(QStringLiteral("Arial"), 9, header ? QFont::Bold : QFont::Normal);
            m_p.setFont(f);
            m_p.setPen(header ? Qt::white : Qt::black);

            int x = 0;
            for (int i = 0; i < cells.size() && i < widths.size(); ++i) {
                const int w = int(widths.at(i) * m_width);
                const QRect cell(x + h / 5, y, w - 2 * h / 5, h);
                const QString t = QFontMetrics(f, &m_writer).elidedText(cells.at(i), Qt::ElideRight, cell.width());
                m_p.drawText(cell, Qt::AlignVCenter | Qt::AlignLeft, t);
                x += w;
            }
            m_p.setPen(Qt::black);
            y += h;
        };

        drawRow(headers, true, false);
        bool shaded = false;
        for (const QStringList& r : rows) {
            if (y + h > m_height - footerHeight()) {
                ensure(h * 2);
                drawRow(headers, true, false);   // répète l'en-tête sur la nouvelle page
            }
            drawRow(r, false, shaded);
            shaded = !shaded;
        }
        y += h / 2;
    }

    void image(const QImage& img)
    {
        if (img.isNull())
            return;
        const int w = m_width;
        const int h = int(double(img.height()) * w / img.width());
        ensure(h);
        m_p.drawImage(QRect(0, y, w, h), img);
        y += h + 20;
    }

    void footer()
    {
        m_p.setFont(QFont(QStringLiteral("Arial"), 8));
        m_p.setPen(Qt::gray);
        const int h = footerHeight();
        m_p.drawText(QRect(0, m_height - h, m_width, h), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("%1 %2 – %3")
                         .arg(AppInfo::name(), AppInfo::version(),
                              QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))));
        m_p.drawText(QRect(0, m_height - h, m_width, h), Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("Page %1").arg(m_page));
        m_p.setPen(Qt::black);
    }

private:
    int footerHeight() const
    {
        return QFontMetrics(QFont(QStringLiteral("Arial"), 8), &m_writer).height() * 2;
    }

    QPdfWriter& m_writer;
    QPainter& m_p;
    int m_width = 0;
    int m_height = 0;
    int m_page = 1;
};

QString fmt(double v, int decimals)
{
    return VehicleSample::valid(v) ? QString::number(v, 'f', decimals).replace(QLatin1Char('.'), QLatin1Char(','))
                                   : QStringLiteral("–");
}
}

bool PdfExporter::exportReport(const QString& filename, const ReportData& d, QString* error)
{
    QPdfWriter writer(filename);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setResolution(150);
    writer.setTitle(d.title);
    writer.setCreator(AppInfo::name());

    QPainter painter;
    if (!painter.begin(&writer)) {
        if (error)
            *error = QStringLiteral("Impossible de créer le fichier PDF : %1").arg(filename);
        return false;
    }
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    PdfPage page(writer, painter);

    page.title(d.title);

    // --- Véhicule -------------------------------------------------------
    page.section(QStringLiteral("Véhicule"));
    page.line(QStringLiteral("VIN"), d.vehicle.vin.isEmpty() ? QStringLiteral("non lu") : d.vehicle.vin);
    if (!d.vehicle.brand.isEmpty() || !d.vehicle.model.isEmpty())
        page.line(QStringLiteral("Modèle"), QStringLiteral("%1 %2").arg(d.vehicle.brand, d.vehicle.model));
    if (!d.vehicle.engine.isEmpty())
        page.line(QStringLiteral("Motorisation"), d.vehicle.engine);
    if (!d.vehicle.registration.isEmpty())
        page.line(QStringLiteral("Immatriculation"), d.vehicle.registration);
    if (d.vehicle.mileage > 0)
        page.line(QStringLiteral("Kilométrage"), QStringLiteral("%1 km").arg(d.vehicle.mileage));
    if (!d.adapter.isEmpty())
        page.line(QStringLiteral("Adaptateur"), d.adapter);
    if (!d.protocol.isEmpty())
        page.line(QStringLiteral("Protocole"), d.protocol);
    if (!d.sessionLabel.isEmpty())
        page.line(QStringLiteral("Session"), d.sessionLabel);
    if (!d.samples.isEmpty()) {
        page.line(QStringLiteral("Période"),
                  QStringLiteral("%1 → %2 (%3 mesures)")
                      .arg(d.samples.first().timestamp.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")),
                           d.samples.last().timestamp.toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")))
                      .arg(d.samples.size()));
    }

    // --- Défauts --------------------------------------------------------
    page.section(QStringLiteral("Codes défauts"));
    if (d.dtcs.isEmpty()) {
        page.text(QStringLiteral("Aucun code défaut mémorisé lors de la dernière lecture."));
    } else {
        QList<QStringList> rows;
        for (const DTC& dtc : d.dtcs)
            rows << QStringList{ dtc.code, dtc.description, DTC::statusText(dtc.status) };
        page.table({ QStringLiteral("Code"), QStringLiteral("Description"), QStringLiteral("État") },
                   rows, { 0.15, 0.65, 0.20 });
    }

    // --- Statistiques ---------------------------------------------------
    if (!d.samples.isEmpty()) {
        page.section(QStringLiteral("Statistiques"));
        QList<QStringList> rows;
        for (const SeriesStats& st : StatisticsManager::computeAll(d.samples)) {
            if (st.count == 0)
                continue;
            const int dec = st.unit == QLatin1String("V") || st.unit == QLatin1String("g/s") ? 2 : 1;
            rows << QStringList{ st.name, st.unit, fmt(st.min, dec), fmt(st.max, dec),
                                 fmt(st.average, dec), QString::number(st.count) };
        }
        page.table({ QStringLiteral("Grandeur"), QStringLiteral("Unité"), QStringLiteral("Min"),
                     QStringLiteral("Max"), QStringLiteral("Moyenne"), QStringLiteral("Mesures") },
                   rows, { 0.34, 0.12, 0.13, 0.13, 0.15, 0.13 });
    }

    // --- Graphique ------------------------------------------------------
    if (!d.chart.isNull()) {
        page.section(QStringLiteral("Graphique"));
        page.image(d.chart);
    }

    // --- Mesures détaillées ------------------------------------------
    if (!d.samples.isEmpty()) {
        page.section(QStringLiteral("Mesures détaillées"));

        // Sous-échantillonnage régulier si la session est longue.
        const int total = int(d.samples.size());
        const int step = qMax(1, (total + d.maxTableRows - 1) / qMax(1, d.maxTableRows));
        if (step > 1)
            page.text(QStringLiteral("Une mesure sur %1 est affichée (%2 au total).").arg(step).arg(total));

        QList<QStringList> rows;
        for (int i = 0; i < total; i += step) {
            const VehicleSample& s = d.samples.at(i);
            rows << QStringList{ s.timestamp.toString(QStringLiteral("HH:mm:ss")),
                                 fmt(s.rpm, 0), fmt(s.speed, 0), fmt(s.coolant, 0),
                                 fmt(s.throttle, 1), fmt(s.engineLoad, 1), fmt(s.batteryVoltage, 1) };
        }
        page.table({ QStringLiteral("Heure"), QStringLiteral("tr/min"), QStringLiteral("km/h"),
                     QStringLiteral("Temp °C"), QStringLiteral("Papillon %"), QStringLiteral("Charge %"),
                     QStringLiteral("Tension V") },
                   rows, { 0.16, 0.14, 0.12, 0.14, 0.15, 0.14, 0.15 });
    }

    page.footer();
    painter.end();
    return true;
}
