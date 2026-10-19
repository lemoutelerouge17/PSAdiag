#include "App/MainWindow.h"
#include "Core/AppInfo.h"
#include "Core/Logger.h"
#include "Utils/ThemeManager.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(AppInfo::name());
    QApplication::setOrganizationName(AppInfo::organization());
    QApplication::setApplicationVersion(AppInfo::version());

    // Boîtes de dialogue standard Qt en français.
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(QLocale::French), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    ThemeManager::apply(ThemeManager::saved());

    MainWindow window;
    window.show();

    const int code = app.exec();
    Logger::info(QStringLiteral("Fermeture de l'application"));
    return code;
}
