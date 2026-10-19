@echo off
REM ==========================================================================
REM  Compilation Release + déploiement Qt + installateur Inno Setup (Windows)
REM
REM  Prérequis : Qt 6.5+ (MSVC 2019/2022 64 bits ou MinGW 64 bits), CMake,
REM              Inno Setup 6 (iscc dans le PATH) pour l'installateur.
REM  Adapter QTDIR ci-dessous si nécessaire.
REM ==========================================================================
setlocal

if "%QTDIR%"=="" set QTDIR=C:\Qt\6.7.3\msvc2019_64

echo === Qt : %QTDIR%
set PATH=%QTDIR%\bin;%PATH%

echo === Configuration CMake
cmake -S . -B build -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release -DPSADIAG_BUILD_TESTS=ON
if errorlevel 1 goto error

echo === Compilation
cmake --build build --config Release --parallel
if errorlevel 1 goto error

echo === Tests unitaires
ctest --test-dir build -C Release --output-on-failure

echo === Preparation du dossier Release
if exist Release rmdir /s /q Release
mkdir Release

if exist build\Release\PSADiag.exe (
    copy build\Release\PSADiag.exe Release\ >nul
) else (
    copy build\PSADiag.exe Release\ >nul
)
if errorlevel 1 goto error

windeployqt --release --compiler-runtime Release\PSADiag.exe
if errorlevel 1 goto error

where iscc >nul 2>nul
if errorlevel 1 (
    echo.
    echo Inno Setup ^(iscc^) introuvable : installateur non genere.
    echo L'application est prete dans le dossier Release\
    goto end
)

echo === Installateur
iscc Installer\PSADiag.iss
if errorlevel 1 goto error

echo.
echo Installateur genere dans le dossier Output\
goto end

:error
echo.
echo *** ECHEC de la compilation ***
exit /b 1

:end
endlocal
pause
