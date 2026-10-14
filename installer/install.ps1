$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'RemoteAssist.exe'
if (!(Test-Path -LiteralPath $source)) { throw 'RemoteAssist.exe missing.' }
if (![Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
$hashFile = Join-Path $PSScriptRoot 'RemoteAssist.sha256'
if (!(Test-Path -LiteralPath $hashFile)) { throw 'Package checksum missing. Extract the entire archive.' }
$expectedHash = (Get-Content -LiteralPath $hashFile -Raw).Trim()
if ($expectedHash -notmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $expectedHash) { throw 'Package damaged: checksum mismatch.' }
if (Get-Process -Name 'RemoteAssist','RemoteAssist-localfix' -ErrorAction SilentlyContinue) { throw 'Close all Remote Assist windows before installation.' }
$destination = Join-Path $env:LOCALAPPDATA 'Programs/RemoteAssist'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
Copy-Item -LiteralPath $source -Destination (Join-Path $destination 'RemoteAssist.exe') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'uninstall.ps1') -Destination $destination -Force
$shell = New-Object -ComObject WScript.Shell
$menu = Join-Path ([Environment]::GetFolderPath('Programs')) 'Remote Assist'
New-Item -ItemType Directory -Path $menu -Force | Out-Null
$shortcut = $shell.CreateShortcut((Join-Path $menu 'Remote Assist.lnk'))
$shortcut.TargetPath = Join-Path $destination 'RemoteAssist.exe'
$shortcut.WorkingDirectory = $destination
$shortcut.Save()
$uninstallShortcut = $shell.CreateShortcut((Join-Path $menu 'Uninstall Remote Assist.lnk'))
$uninstallShortcut.TargetPath = Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
$uninstallShortcut.Arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $destination 'uninstall.ps1') + '"'
$uninstallShortcut.WorkingDirectory = $destination
$uninstallShortcut.Save()
Write-Host ('Installed: ' + $destination)
Write-Host 'Open or uninstall Remote Assist from the Start menu.'
