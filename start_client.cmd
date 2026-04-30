@ECHO OFF
REM Launches only the Knight Online client.

SET "ROOT=%~dp0"
SET "BIN=%ROOT%bin\Debug-x64"
SET "CLIENT_CWD=%ROOT%assets\Client"

IF NOT EXIST "%BIN%\KnightOnLine.exe" (
    ECHO ERROR: KnightOnLine.exe not found at "%BIN%". Build Client.slnx first.
    EXIT /B 1
)

ECHO Starting Client (KnightOnLine.exe)...
START "KnightOnLine" /D "%CLIENT_CWD%" "%BIN%\KnightOnLine.exe"
