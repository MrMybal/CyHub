#ifndef AppVersion
  #error Define AppVersion from the CyHub CMake project
#endif
#ifndef PackageDir
  #error Define PackageDir from a fresh CMake install
#endif

[Setup]
AppId={{822B880C-CFA7-44BD-9EA4-D69930730C53}
AppName=CyHub
AppVersion={#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyHub
AppUpdatesURL=https://github.com/MrMybal/CyHub/releases
DefaultDirName={autopf}\CyHub
DefaultGroupName=CyHub
DisableDirPage=auto
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputBaseFilename=CyHub-Setup-windows-x64
SetupIconFile=..\assets\branding\cyhub.ico
UninstallDisplayIcon={app}\CyHub.exe
LicenseFile={#PackageDir}\LICENSE
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[CustomMessages]
english.ServiceStopFailed=Close CyHub and stop the CyHub Updater service before retrying.
french.ServiceStopFailed=Fermez CyHub et arrêtez le service CyHub Updater avant de réessayer.
english.ServiceSetupFailed=CyHub Updater could not be configured. See the installation log for details.
french.ServiceSetupFailed=Impossible de configurer CyHub Updater. Consultez le journal d’installation pour en savoir plus.

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\CyHub"; Filename: "{app}\CyHub.exe"

[Run]
Filename: "{app}\CyHub.exe"; Description: "{cm:LaunchProgram,CyHub}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File ""{app}\Uninstall-CyHubService.ps1"""; Check: UsesService; Flags: runhidden; RunOnceId: "CyHubUpdater"

[Code]
var
  RestartService: Boolean;

function UsesService: Boolean;
begin
  Result := IsAdminInstallMode and
    (CompareText(ExpandConstant('{app}'), ExpandConstant('{commonpf64}\CyHub')) = 0);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
  ImagePath: String;
begin
  Result := '';
  if UsesService and RegQueryStringValue(HKLM, 'SYSTEM\CurrentControlSet\Services\CyHubUpdater',
    'ImagePath', ImagePath) then
  begin
    if Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      '-NoProfile -NonInteractive -Command "$ErrorActionPreference=''Stop''; $s=Get-Service CyHubUpdater; ' +
      'if($s.Status -eq ''Stopped''){exit 2}; Stop-Service CyHubUpdater; ' +
      '$s.WaitForStatus(''Stopped'',[TimeSpan]::FromSeconds(35))"', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then
    begin
      if ExitCode = 0 then RestartService := True
      else if ExitCode <> 2 then Result := CustomMessage('ServiceStopFailed');
    end
    else Result := CustomMessage('ServiceStopFailed');
  end;
end;

procedure DeinitializeSetup;
var
  ExitCode: Integer;
begin
  if RestartService then
    Exec(ExpandConstant('{sys}\sc.exe'), 'start CyHubUpdater', '', SW_HIDE, ewWaitUntilTerminated, ExitCode);
end;

procedure CurStepChanged(Step: TSetupStep);
var
  ExitCode: Integer;
begin
  if (Step = ssPostInstall) and UsesService then
  begin
    if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      '-NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "' +
      ExpandConstant('{app}\Install-CyHub.ps1') + '"', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) or
      (ExitCode <> 0) then
      RaiseException(CustomMessage('ServiceSetupFailed'));
    RestartService := False;
  end;
end;
