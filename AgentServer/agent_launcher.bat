@echo off
setlocal

set "CURRENT_DIR=%~dp0"

for %%I in ("%CURRENT_DIR%..\runtimes\win-x64\native") do set "DLL_PATH=%%~fI"

set "PATH=%DLL_PATH%;%PATH%"

start /b "" "%CURRENT_DIR%MaaPCRAgentServer.exe" %*

endlocal