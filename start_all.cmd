@ECHO OFF
REM Local development launcher for OpenKO (Docker DB)
REM Starts servers in order, then the client.

SET "ROOT=%~dp0"
SET "BIN=%ROOT%bin\Debug-x64"
SET "MAP=%ROOT%assets\Server\MAP"
SET "QUESTS=%ROOT%assets\Server\QUESTS"
SET "CLIENT_CWD=%ROOT%assets\Client"

IF NOT EXIST "%BIN%\Ebenezer.exe" (
    ECHO ERROR: Build artifacts not found at "%BIN%". Build All.slnx first.
    EXIT /B 1
)

ECHO Starting Aujard...
START "Aujard" /D "%BIN%" "%BIN%\Aujard.exe"
TIMEOUT /T 3 /NOBREAK >NUL

ECHO Starting Ebenezer...
START "Ebenezer" /D "%BIN%" "%BIN%\Ebenezer.exe" --map-dir="%MAP%" --quests-dir="%QUESTS%"
TIMEOUT /T 3 /NOBREAK >NUL

ECHO Starting AIServer...
START "AIServer" /D "%BIN%" "%BIN%\AIServer.exe" --map-dir="%MAP%" --event-dir="%MAP%"
TIMEOUT /T 2 /NOBREAK >NUL

ECHO Starting VersionManager...
START "VersionManager" /D "%BIN%" "%BIN%\VersionManager.exe"
TIMEOUT /T 2 /NOBREAK >NUL

ECHO Starting ItemManager...
START "ItemManager" /D "%BIN%" "%BIN%\ItemManager.exe"
TIMEOUT /T 2 /NOBREAK >NUL

ECHO Starting Client (KnightOnLine.exe)...
START "KnightOnLine" /D "%CLIENT_CWD%" "%BIN%\KnightOnLine.exe"

ECHO.
ECHO All processes launched. Each runs in its own console window.
ECHO Test login: testing / testing
