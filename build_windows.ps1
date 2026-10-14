param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Build Tools are not installed.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Install the Desktop development with C++ workload.' }
$cmake = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (!(Test-Path -LiteralPath $cmake)) { throw 'Install C++ CMake tools for Windows.' }
function Invoke-BuildTool([string[]]$ToolArguments) {
    $processInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $processInfo.FileName = $cmake
    $processInfo.WorkingDirectory = $PSScriptRoot
    $processInfo.UseShellExecute = $false
    # Normalize names: duplicate Path/PATH entries can break MSBuild.
    $processInfo.Environment.Clear()
    Get-ChildItem Env: | ForEach-Object { $processInfo.Environment[$_.Name.ToUpperInvariant()] = $_.Value }
    foreach ($argument in $ToolArguments) { $processInfo.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::Start($processInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw ('CMake failed: ' + $process.ExitCode) }
}
Invoke-BuildTool @('-S', '.', '-B', 'build-windows', '-A', 'x64')
Invoke-BuildTool @('--build', 'build-windows', '--config', $Configuration, '--parallel')
