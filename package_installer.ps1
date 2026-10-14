$ErrorActionPreference = 'Stop'
$binary = Join-Path $PSScriptRoot 'dist/RemoteAssist.exe'
$folder = Join-Path $PSScriptRoot 'installer'
Copy-Item -LiteralPath $binary -Destination (Join-Path $folder 'RemoteAssist.exe') -Force
(Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash | Set-Content -LiteralPath (Join-Path $folder 'RemoteAssist.sha256') -Encoding ASCII
$names = @('RemoteAssist.exe','RemoteAssist.sha256','Install.cmd','install.ps1','uninstall.ps1','README.txt')
$files = $names | ForEach-Object { Join-Path $folder $_ }
foreach ($name in @('install.ps1','uninstall.ps1')) {
    $tokens = $null; $parseErrors = $null
    [System.Management.Automation.Language.Parser]::ParseFile((Join-Path $folder $name),[ref]$tokens,[ref]$parseErrors) | Out-Null
    if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
}
$archive = Join-Path $PSScriptRoot 'dist/RemoteAssist-installer-win64.zip'
Compress-Archive -LiteralPath $files -DestinationPath $archive -Force
(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'dist/RemoteAssist-installer.sha256') -Encoding ASCII
Write-Output $archive
