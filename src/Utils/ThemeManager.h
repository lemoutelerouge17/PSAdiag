#pragma once

#include <QString>

class QApplication;

class ThemeManager
{
public:
    enum class Theme { Dark, Light };

    static void apply(Theme theme);
    static void applyDarkTheme(QApplication& app);
    static void applyLightTheme(QApplication& app);

    static Theme current();
    static Theme saved();        // QSettings "theme" (sombre par défaut)
    static void save(Theme theme);

    static bool isDark() { return current() == Theme::Dark; }

private:
    static Theme s_current;
};
