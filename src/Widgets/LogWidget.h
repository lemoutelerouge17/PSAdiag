#pragma once

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// Journal horodaté + terminal de commandes brutes (AT / OBD-II).
class LogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogWidget(QWidget* parent = nullptr);

    void setTerminalEnabled(bool enabled);

public slots:
    void appendLine(int level, const QString& line);

signals:
    void commandEntered(const QString& command);

private:
    QPlainTextEdit* m_view;
    QLineEdit* m_input;
    QPushButton* m_send;
    QCheckBox* m_showTraffic;
};
