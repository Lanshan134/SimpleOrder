@echo off
setlocal EnableExtensions
rem Native cmd build entry. Does not invoke PowerShell or change its policies.
for %%I in ("%~dp0.") do set "PROJECT_ROOT=%%~fI"
set "MINGW_BIN=D:\CodeApp\Environment\mingw64\bin"
if not "%~2"=="" set "MINGW_BIN=%~2"
if not exist "%MINGW_BIN%\mingw32-make.exe" (
    echo ERROR: MinGW not found. Usage: build.cmd [Qt-root] [MinGW-bin]
    exit /b 1
)
set "BUILD_ALIAS=%TEMP%\SimpleOrder-cmd-%RANDOM%-%RANDOM%"
mklink /J "%BUILD_ALIAS%" "%PROJECT_ROOT%" >nul
if errorlevel 1 (
    echo ERROR: Cannot create the temporary directory junction.
    exit /b 1
)
set "QT_ROOT=%BUILD_ALIAS%\work\qt\5.15.2\mingw81_64"
if not "%~1"=="" set "QT_ROOT=%~1"
if not exist "%QT_ROOT%\bin\qmake.exe" (
    echo ERROR: Qt not found. Usage: build.cmd [Qt-root] [MinGW-bin]
    exit /b 1
)
set "PATH=%MINGW_BIN%;%PATH%"
if not exist "%BUILD_ALIAS%\build" mkdir "%BUILD_ALIAS%\build"
"%QT_ROOT%\bin\qmake.exe" -o "%BUILD_ALIAS%\build\Makefile" "%BUILD_ALIAS%\taskmanager.pro"
if errorlevel 1 goto failed
"%MINGW_BIN%\mingw32-make.exe" -C "%BUILD_ALIAS%\build" -j4
if errorlevel 1 goto failed
set "APP_DIR=%BUILD_ALIAS%\outputs\SimpleOrder"
if not exist "%APP_DIR%" mkdir "%APP_DIR%"
copy /Y "%BUILD_ALIAS%\build\release\SimpleOrder.exe" "%APP_DIR%\SimpleOrder.exe" >nul
if errorlevel 1 goto failed
copy /Y "%BUILD_ALIAS%\style.qss" "%APP_DIR%\style.qss" >nul
if errorlevel 1 goto failed
for %%F in (Qt5Core.dll Qt5Gui.dll Qt5Sql.dll Qt5Widgets.dll) do (
    copy /Y "%QT_ROOT%\bin\%%F" "%APP_DIR%\%%F" >nul
    if errorlevel 1 goto failed
)
for %%D in (platforms sqldrivers styles) do if not exist "%APP_DIR%\%%D" mkdir "%APP_DIR%\%%D"
for %%F in (qwindows.dll qoffscreen.dll) do (
    copy /Y "%QT_ROOT%\plugins\platforms\%%F" "%APP_DIR%\platforms\%%F" >nul
    if errorlevel 1 goto failed
)
copy /Y "%QT_ROOT%\plugins\sqldrivers\qsqlite.dll" "%APP_DIR%\sqldrivers\qsqlite.dll" >nul
if errorlevel 1 goto failed
copy /Y "%QT_ROOT%\plugins\styles\qwindowsvistastyle.dll" "%APP_DIR%\styles\qwindowsvistastyle.dll" >nul
if errorlevel 1 goto failed
for %%F in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    copy /Y "%MINGW_BIN%\%%F" "%APP_DIR%\%%F" >nul
    if errorlevel 1 goto failed
)
echo BUILD OK: outputs\SimpleOrder\SimpleOrder.exe
exit /b 0
:failed
echo BUILD FAILED. If copying the EXE failed, exit the running app and retry.
exit /b 1
