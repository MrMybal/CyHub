param(
    [Parameter(Mandatory)] [string] $BuildDirectory,
    [Parameter(Mandatory)] [string] $Iscc,
    [string] $OutputDirectory = 'dist'
)

$ErrorActionPreference = 'Stop'
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$compiler = (Resolve-Path -LiteralPath $Iscc).Path
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$versionEntry = Select-String -LiteralPath (Join-Path $build 'CMakeCache.txt') -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(\d+\.\d+\.\d+)$'
if (-not $versionEntry) { throw 'Configure the Windows release build before creating the installer.' }
$version = $versionEntry.Matches[0].Groups[1].Value
$stage = Join-Path $build ('installer-' + [Guid]::NewGuid().ToString('N'))

& cmake --install $build --config Release --component CyHub --prefix $stage
if ($LASTEXITCODE) { throw 'Staging the CyHub distribution failed.' }
& $compiler "/DAppVersion=$version" "/DPackageDir=$stage" "/O$output" (Join-Path $PSScriptRoot 'CyHub.iss')
if ($LASTEXITCODE) { throw 'Inno Setup could not build the CyHub installer.' }
Get-Item -LiteralPath (Join-Path $output 'CyHub-Setup-windows-x64.exe')
