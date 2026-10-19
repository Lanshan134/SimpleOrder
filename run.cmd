@echo off
setlocal
set "APP=%~dp0outputs\SimpleOrder\SimpleOrder.exe"
if not exist "%APP%" (
    echo Program not found. Run build.cmd first.
    exit /b 1
)
start "" "%APP%"
exit /b 0
