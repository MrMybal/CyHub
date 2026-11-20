#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'
$name = 'CyHubUpdater'
$service = Get-Service -Name $name -ErrorAction SilentlyContinue
if ($service) {
    if ($service.Status -ne 'Stopped') {
        Stop-Service -Name $name
        $service.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(35))
    }
    $service.Dispose()
    & sc.exe delete $name | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Suppression du service impossible.' }
}
Write-Host 'Service retiré. CyHub et les versions installées restent dans Program Files.'
