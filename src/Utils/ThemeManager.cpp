#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

ThemeManager::Theme ThemeManager::s_current = ThemeManager::Theme::Dark;

void ThemeManager::apply(Theme theme)
{
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app)
        return;
    if (theme == Theme::Dark)
        applyDarkTheme(*app);
    else
        applyLightTheme(*app);
}

void ThemeManager::applyDarkTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    const QColor window(32, 33, 36);
    const QColor base(43, 43, 46);
    const QColor text(230, 230, 230);
    const QColor accent(66, 133, 244);

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, QColor(52, 53, 57));
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(60, 64, 67));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));

    app.setPalette(p);
    app.setStyleSheet(QStringLiteral(
        "QToolTip { color: #e6e6e6; background-color: #2b2b2e; border: 1px solid #555; }"
        "QTabBar::tab { padding: 6px 14px; }"
        "QGroupBox { font-weight: bold; }"));

    s_current = Theme::Dark;
}

void ThemeManager::applyLightTheme(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setPalette(app.style()->standardPalette());
    app.setStyleSheet(QStringLiteral(
        "QTabBar::tab { padding: 6px 14px; }"
        "QGroupBox { font-weight: bold; }"));

    s_current = Theme::Light;
}

ThemeManager::Theme ThemeManager::current()
{
    return s_current;
}

ThemeManager::Theme ThemeManager::saved()
{
    QSettings cfg;
    return cfg.value(QStringLiteral("theme"), QStringLiteral("dark")).toString() == QLatin1String("light")
               ? Theme::Light
               : Theme::Dark;
}

void ThemeManager::save(Theme theme)
{
    QSettings cfg;
    cfg.setValue(QStringLiteral("theme"),
                 theme == Theme::Light ? QStringLiteral("light") : QStringLiteral("dark"));
}
