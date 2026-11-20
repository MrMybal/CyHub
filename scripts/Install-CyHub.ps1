#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

if (-not [Environment]::Is64BitProcess) {
    throw 'Exécute ce script depuis PowerShell x64.'
}
$name = 'CyHubUpdater'
$root = Join-Path ([Environment]::GetFolderPath('ProgramFiles')) 'CyHub'
$managed = Join-Path $root 'Managed'
$files = 'CyHub.exe', 'CyHubMCP.exe', 'CyHubUpdater.exe', 'catalog.json'
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $file) -PathType Leaf)) {
        throw "Fichier manquant dans le paquet CyHub. $file"
    }
}

foreach ($directory in @($root, $managed)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    if ((Get-Item -LiteralPath $directory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "Dossier de destination redirigé. $directory"
    }
}

$service = Get-Service -Name $name -ErrorAction SilentlyContinue
if ($service -and $service.Status -ne 'Stopped') {
    Stop-Service -Name $name
    $service.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(35))
}

if ($PSScriptRoot -ine $root) {
    foreach ($file in $files) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $root -Force
    }
    foreach ($optional in @('skills', 'assets', 'Documentation', 'licenses', 'LICENSE', 'README.md', 'README.fr.md', 'VERSION.md', 'VERSION.fr.md', 'Install-CyHub.ps1', 'Uninstall-CyHubService.ps1')) {
        $source = Join-Path $PSScriptRoot $optional
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination $root -Recurse -Force
        }
    }
}

$path = '"{0}"' -f (Join-Path $root 'CyHubUpdater.exe')
if (-not $service) {
    New-Service -Name $name -DisplayName 'CyHub Updater' -BinaryPathName $path -StartupType Disabled | Out-Null
}
$registration = Get-CimInstance -ClassName Win32_Service -Filter "Name='$name'"
$change = Invoke-CimMethod -InputObject $registration -MethodName Change -Arguments @{
    PathName = $path
    StartName = 'NT AUTHORITY\LocalService'
    StartPassword = ''
    StartMode = 'Manual'
}
if ($change.ReturnValue -ne 0) { throw "Configuration du service impossible. Code $($change.ReturnValue)" }

& sc.exe sidtype $name unrestricted | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Configuration de l’identité du service impossible." }

& icacls.exe $managed /grant:r "NT SERVICE\${name}:(OI)(CI)M" | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Attribution des droits de stockage impossible.' }

Set-Service -Name $name -StartupType Automatic
Start-Service -Name $name
(Get-Service -Name $name).WaitForStatus('Running', [TimeSpan]::FromSeconds(20))
Write-Host "CyHub est installé dans $root."
