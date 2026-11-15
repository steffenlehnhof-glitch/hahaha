@echo off
rem Baut PurpleSpice.vst3 unter Windows.
rem Voraussetzung: Visual Studio 2022 (Workload "Desktopentwicklung mit C++"), CMake und Git.
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo CMake wurde nicht gefunden. Bitte von https://cmake.org/download/ installieren
    echo und bei der Installation "Add CMake to the system PATH" anhaken.
    pause
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo Git wurde nicht gefunden. Bitte von https://git-scm.com/ installieren.
    echo Git wird gebraucht, um JUCE automatisch herunterzuladen.
    pause
    exit /b 1
)

echo === Konfigurieren (laedt beim ersten Mal JUCE herunter, das dauert etwas) ===
cmake -B build -A x64
if errorlevel 1 goto :fail

echo === Bauen ===
cmake --build build --config Release --parallel
if errorlevel 1 goto :fail

echo.
echo Fertig! Das Plugin liegt hier:
echo   %~dp0build\PurpleSpice_artefacts\Release\VST3\Purple Spice.vst3
echo.
echo Kopiere den ganzen Ordner "Purple Spice.vst3" nach:
echo   C:\Program Files\Common Files\VST3
echo und lass FL Studio danach neu nach Plugins suchen.
pause
exit /b 0

:fail
echo.
echo Der Build ist fehlgeschlagen. Die Fehlermeldung steht weiter oben.
pause
exit /b 1
