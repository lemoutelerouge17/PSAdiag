#include "LogWidget.h"

#include "Core/AppInfo.h"
#include "Core/Logger.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QUrl>
#include <QVBoxLayout>

LogWidget::LogWidget(QWidget* parent)
    : QWidget(parent)
{
    m_view = new QPlainTextEdit;
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(5000);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    m_view->setFont(mono);

    m_showTraffic = new QCheckBox(tr("Afficher les échanges TX/RX"));
    m_showTraffic->setChecked(false);

    auto* clear = new QPushButton(tr("Effacer"));
    auto* openFolder = new QPushButton(tr("Ouvrir le dossier des journaux"));

    m_input = new QLineEdit;
    m_input->setPlaceholderText(tr("Commande brute (ex. ATI, ATRV, 010C, 0902)…"));
    // Commandes de lecture uniquement : AT/ST de configuration et requêtes OBD-II standard.
    m_input->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9 ]{0,24}")), m_input));
    m_input->setFont(mono);
    m_send = new QPushButton(tr("Envoyer"));

    auto* top = new QHBoxLayout;
    top->addWidget(m_showTraffic);
    top->addStretch();
    top->addWidget(clear);
    top->addWidget(openFolder);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(new QLabel(tr("Terminal :")));
    bottom->addWidget(m_input, 1);
    bottom->addWidget(m_send);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_view, 1);
    layout->addLayout(bottom);

    auto send = [this]() {
        const QString cmd = m_input->text().trimmed();
        if (cmd.isEmpty())
            return;
        emit commandEntered(cmd);
        m_input->clear();
    };
    connect(m_send, &QPushButton::clicked, this, send);
    connect(m_input, &QLineEdit::returnPressed, this, send);
    connect(clear, &QPushButton::clicked, m_view, &QPlainTextEdit::clear);
    connect(openFolder, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppInfo::logDir()));
    });

    connect(&Logger::instance(), &Logger::messageLogged, this, &LogWidget::appendLine);

    setTerminalEnabled(false);
}

void LogWidget::setTerminalEnabled(bool enabled)
{
    m_input->setEnabled(enabled);
    m_send->setEnabled(enabled);
}

void LogWidget::appendLine(int level, const QString& line)
{
    const auto lvl = static_cast<Logger::Level>(level);
    const bool traffic = lvl == Logger::Level::Tx || lvl == Logger::Level::Rx;
    if (traffic && !m_showTraffic->isChecked())
        return;
    if (lvl == Logger::Level::Debug && !m_showTraffic->isChecked())
        return;

    QString color;
    switch (lvl) {
    case Logger::Level::Warning: color = QStringLiteral("#e0a020"); break;
    case Logger::Level::Error:   color = QStringLiteral("#e04848"); break;
    case Logger::Level::Tx:      color = QStringLiteral("#4a90e2"); break;
    case Logger::Level::Rx:      color = QStringLiteral("#3cb371"); break;
    case Logger::Level::Debug:   color = QStringLiteral("#888888"); break;
    default: break;
    }

    QScrollBar* bar = m_view->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;

    const QString escaped = line.toHtmlEscaped();
    m_view->appendHtml(color.isEmpty()
                           ? QStringLiteral("<span>%1</span>").arg(escaped)
                           : QStringLiteral("<span style=\"color:%1\">%2</span>").arg(color, escaped));

    if (atBottom)
        bar->setValue(bar->maximum());
}
