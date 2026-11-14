param([string]$Executable = "$PSScriptRoot/../outputs/SimpleOrder/SimpleOrder.exe")
$ErrorActionPreference = 'Stop'
$env:QT_QPA_PLATFORM = 'windows'
# 自检内部使用 WA_DontShowOnScreen，隔离数据库写在当前目录 test-results 下。
$result = Start-Process -FilePath (Resolve-Path $Executable) -ArgumentList '--self-test' -Wait -PassThru -WindowStyle Hidden
if ($result.ExitCode -ne 0) { throw "自检失败：$($result.ExitCode)" }
Write-Host 'PASS：检查 test-results 最新目录中的 result.txt 与截图。'
