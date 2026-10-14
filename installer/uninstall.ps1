$ErrorActionPreference = 'Stop'
$expected = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs/RemoteAssist'))
if ([IO.Path]::GetFullPath($PSScriptRoot) -ne $expected) { throw 'Run the installed uninstaller.' }
if (Get-Process -Name 'RemoteAssist' -ErrorAction SilentlyContinue) { throw 'Close Remote Assist before uninstalling.' }
foreach ($name in @('RemoteAssist.exe', 'uninstall.ps1')) {
    $item = Join-Path $expected $name
    if (Test-Path -LiteralPath $item) { Remove-Item -LiteralPath $item -Force }
}
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'Remote Assist/Remote Assist.lnk'
if (Test-Path -LiteralPath $shortcut) { Remove-Item -LiteralPath $shortcut -Force }
$uninstallShortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'Remote Assist/Uninstall Remote Assist.lnk'
if (Test-Path -LiteralPath $uninstallShortcut) { Remove-Item -LiteralPath $uninstallShortcut -Force }
Write-Host 'Removed. Session logs preserved.'
