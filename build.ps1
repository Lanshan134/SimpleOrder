param(
    [string]$QtRoot = "$PSScriptRoot/work/qt/5.15.2/mingw81_64",
    [string]$MinGWBin = 'D:/CodeApp/Environment/mingw64/bin'
)
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath "$QtRoot/bin/qmake.exe")) { throw '请用 -QtRoot 指定 Qt 5.15.2 MinGW 8.1 64 位安装目录。' }
if (!(Test-Path -LiteralPath "$MinGWBin/mingw32-make.exe")) { throw '请用 -MinGWBin 指定匹配 Qt 的 MinGW 编译器 bin 目录。' }
$env:PATH = "$MinGWBin;$env:PATH"
# Qt 5 的 qmake 在部分 Windows 代码页下无法处理中文路径；只创建目录别名，不搬动文件。
$alias = Join-Path ([System.IO.Path]::GetTempPath()) ('SimpleOrder-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Junction -Path $alias -Target $PSScriptRoot | Out-Null
New-Item -ItemType Directory -Force "$PSScriptRoot/build" | Out-Null
$relativeQt = 'work/qt/5.15.2/mingw81_64'
$localQt = Join-Path $PSScriptRoot $relativeQt
if ((Test-Path -LiteralPath $localQt) -and ((Resolve-Path -LiteralPath $QtRoot).Path -eq (Resolve-Path -LiteralPath $localQt).Path)) {
    $QtRoot = Join-Path $alias $relativeQt
}
& "$QtRoot/bin/qmake.exe" -o "$alias/build/Makefile" "$alias/taskmanager.pro"
if ($LASTEXITCODE -ne 0) { throw 'qmake 失败' }
& "$MinGWBin/mingw32-make.exe" -C "$alias/build" -j4
if ($LASTEXITCODE -ne 0) { throw '编译失败' }
New-Item -ItemType Directory -Force "$PSScriptRoot/outputs/SimpleOrder" | Out-Null
Copy-Item "$PSScriptRoot/build/release/SimpleOrder.exe" "$PSScriptRoot/outputs/SimpleOrder/SimpleOrder.exe"
Copy-Item "$PSScriptRoot/style.qss" "$PSScriptRoot/outputs/SimpleOrder/style.qss"
foreach ($name in @('Qt5Core.dll','Qt5Gui.dll','Qt5Sql.dll','Qt5Widgets.dll')) {
    Copy-Item "$QtRoot/bin/$name" "$PSScriptRoot/outputs/SimpleOrder/"
}
foreach ($sub in @('platforms','sqldrivers','styles')) {
    New-Item -ItemType Directory -Force "$PSScriptRoot/outputs/SimpleOrder/$sub" | Out-Null
}
foreach ($name in @('qwindows.dll','qoffscreen.dll')) { Copy-Item "$QtRoot/plugins/platforms/$name" "$PSScriptRoot/outputs/SimpleOrder/platforms/" }
Copy-Item "$QtRoot/plugins/sqldrivers/qsqlite.dll" "$PSScriptRoot/outputs/SimpleOrder/sqldrivers/"
Copy-Item "$QtRoot/plugins/styles/qwindowsvistastyle.dll" "$PSScriptRoot/outputs/SimpleOrder/styles/"
foreach ($name in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) { Copy-Item "$MinGWBin/$name" "$PSScriptRoot/outputs/SimpleOrder/" }
Write-Host "编译完成：$PSScriptRoot/outputs/SimpleOrder/SimpleOrder.exe"
Write-Host "构建目录别名：$alias（仅指向源码目录的目录联接）"
