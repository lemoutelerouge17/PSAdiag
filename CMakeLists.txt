cmake_minimum_required(VERSION 3.21)

project(PSADiag VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Export Excel natif (.xlsx) via la bibliothèque QXlsx (téléchargée automatiquement).
# Désactivé par défaut : l'export CSV/PDF fonctionne sans dépendance externe.
option(PSADIAG_WITH_XLSX "Activer l'export Excel via QXlsx" OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Charts
    SerialPort
    PrintSupport
)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp

    src/Core/AppInfo.h
    src/Core/Logger.h            src/Core/Logger.cpp
    src/Core/Transport.h
    src/Core/SerialManager.h     src/Core/SerialManager.cpp
    src/Core/SimulatorTransport.h src/Core/SimulatorTransport.cpp

    src/OBD/ObdCommand.h
    src/OBD/ObdParser.h          src/OBD/ObdParser.cpp
    src/OBD/ObdEngine.h          src/OBD/ObdEngine.cpp

    src/Models/VehicleSample.h
    src/Models/Vehicle.h
    src/Models/Trip.h
    src/Models/DtcTableModel.h   src/Models/DtcTableModel.cpp

    src/DTC/DTC.h
    src/DTC/DTCManager.h         src/DTC/DTCManager.cpp
    src/DTC/DtcDictionary.cpp

    src/Database/DatabaseManager.h  src/Database/DatabaseManager.cpp
    src/Database/MigrationManager.h src/Database/MigrationManager.cpp
    src/Database/BackupManager.h    src/Database/BackupManager.cpp

    src/Alerts/AlarmManager.h    src/Alerts/AlarmManager.cpp
    src/Trips/TripManager.h      src/Trips/TripManager.cpp

    src/Utils/StatisticsManager.h src/Utils/StatisticsManager.cpp
    src/Utils/ThemeManager.h     src/Utils/ThemeManager.cpp

    src/Export/CsvExporter.h     src/Export/CsvExporter.cpp
    src/Export/PdfExporter.h     src/Export/PdfExporter.cpp
    src/Export/ExcelExporter.h   src/Export/ExcelExporter.cpp

    src/Widgets/GaugeWidget.h       src/Widgets/GaugeWidget.cpp
    src/Widgets/ConnectionWidget.h  src/Widgets/ConnectionWidget.cpp
    src/Widgets/DashboardWidget.h   src/Widgets/DashboardWidget.cpp
    src/Widgets/GraphWidget.h       src/Widgets/GraphWidget.cpp
    src/Widgets/DTCWidget.h         src/Widgets/DTCWidget.cpp
    src/Widgets/HistoryWidget.h     src/Widgets/HistoryWidget.cpp
    src/Widgets/StatisticsWidget.h  src/Widgets/StatisticsWidget.cpp
    src/Widgets/TripsWidget.h       src/Widgets/TripsWidget.cpp
    src/Widgets/VehiclesWidget.h    src/Widgets/VehiclesWidget.cpp
    src/Widgets/VehicleDialog.h     src/Widgets/VehicleDialog.cpp
    src/Widgets/AlarmSettingsDialog.h src/Widgets/AlarmSettingsDialog.cpp
    src/Widgets/LogWidget.h         src/Widgets/LogWidget.cpp

    src/App/MainWindow.h         src/App/MainWindow.cpp
)

qt_add_executable(PSADiag WIN32 ${SOURCES})

target_include_directories(PSADiag PRIVATE src)

# Sources en UTF-8 (accents dans les chaînes) avec MSVC.
if(MSVC)
    target_compile_options(PSADiag PRIVATE /utf-8)
endif()

target_compile_definitions(PSADiag PRIVATE
    PSADIAG_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(PSADiag PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Charts
    Qt6::SerialPort
    Qt6::PrintSupport
)

if(PSADIAG_WITH_XLSX)
    include(FetchContent)
    FetchContent_Declare(QXlsx
        GIT_REPOSITORY https://github.com/QtExcel/QXlsx.git
        GIT_TAG        v1.4.9
        SOURCE_SUBDIR  QXlsx
    )
    set(QT_VERSION_MAJOR 6 CACHE STRING "" FORCE)
    FetchContent_MakeAvailable(QXlsx)
    target_link_libraries(PSADiag PRIVATE QXlsx::QXlsx)
    target_compile_definitions(PSADiag PRIVATE PSADIAG_HAS_XLSX)
endif()

# ---------------------------------------------------------------------------
# Tests unitaires du parseur OBD (aucun matériel requis)
# ---------------------------------------------------------------------------
option(PSADIAG_BUILD_TESTS "Compiler les tests unitaires" ON)

if(PSADIAG_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()

    qt_add_executable(tst_obdparser
        Tests/tst_obdparser.cpp
        src/OBD/ObdParser.h  src/OBD/ObdParser.cpp
        src/DTC/DTC.h
        src/DTC/DTCManager.h src/DTC/DTCManager.cpp
        src/DTC/DtcDictionary.cpp
    )
    target_include_directories(tst_obdparser PRIVATE src)
    if(MSVC)
        target_compile_options(tst_obdparser PRIVATE /utf-8)
    endif()
    target_link_libraries(tst_obdparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME tst_obdparser COMMAND tst_obdparser)
endif()
