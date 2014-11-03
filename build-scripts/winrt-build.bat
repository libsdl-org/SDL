@ECHO OFF
REM
REM winrt-build.bat: a batch file to help launch the winrt-build.ps1
REM   Powershell script, either from Windows Explorer, or through Buildbot.
REM
SET ThisScriptsDirectory=%~dp0
SET PowerShellScriptPath=%ThisScriptsDirectory%winrt-build.ps1
PowerShell -NoProfile -ExecutionPolicy Bypass -Command "& '%PowerShellScriptPath%'";