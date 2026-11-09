param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$QtRoot,
    [string]$MinGWBin
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$QtRoot) { $QtRoot = Join-Path $root '.local/qt/5.15.2/mingw81_64' }
if (!$MinGWBin) { $MinGWBin = Join-Path $root '.local/mingw/bin' }
$qmake = Join-Path $QtRoot 'bin/qmake.exe'
$make = Join-Path $MinGWBin 'mingw32-make.exe'
if (!(Test-Path -LiteralPath $qmake) -or !(Test-Path -LiteralPath $make)) {
    throw 'Set up .local/qt and .local/mingw as described in docs/development.md, or pass -QtRoot and -MinGWBin.'
}
$env:PATH = "$MinGWBin;$env:PATH"
$buildDir = Join-Path $root ('build/' + $Configuration.ToLowerInvariant())
$runDir = if ($Configuration -eq 'Debug') { Join-Path $buildDir 'bin' } else { Join-Path $root 'outputs/SimpleOrder' }
New-Item -ItemType Directory -Force -Path $buildDir, $runDir | Out-Null
$qmakeArgs = @('-o', (Join-Path $buildDir 'Makefile'), (Join-Path $root 'taskmanager.pro'),
    'CONFIG-=debug', 'CONFIG-=debug_and_release', 'CONFIG+=release')
if ($Configuration -eq 'Debug') { $qmakeArgs += 'CONFIG+=app_debug' }
& $qmake @qmakeArgs
if ($LASTEXITCODE -ne 0) { throw "qmake failed ($LASTEXITCODE)" }
& $make -C $buildDir -j4
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }

Copy-Item -LiteralPath (Join-Path $buildDir 'SimpleOrder.exe'), (Join-Path $root 'src/ui/theme/style.qss') -Destination $runDir
foreach ($dll in @('Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Sql.dll', 'Qt5Widgets.dll')) {
    Copy-Item -LiteralPath (Join-Path $QtRoot "bin/$dll") -Destination $runDir
}
foreach ($plugin in @('platforms/qwindows.dll', 'platforms/qoffscreen.dll', 'sqldrivers/qsqlite.dll',
    'styles/qwindowsvistastyle.dll')) {
    $pluginDir = Join-Path $runDir (Split-Path -Parent $plugin)
    New-Item -ItemType Directory -Path $pluginDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $QtRoot "plugins/$plugin") -Destination $pluginDir
}
foreach ($dll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
    Copy-Item -LiteralPath (Join-Path $MinGWBin $dll) -Destination $runDir
}
Write-Host "$Configuration build ready: $runDir\SimpleOrder.exe"
