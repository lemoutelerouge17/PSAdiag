; Script Inno Setup 6 – installateur Windows de PSADiag
; Compilation : iscc Installer\PSADiag.iss   (après build_release.bat)

#define MyAppName "PSADiag"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Stephane Cartigny"
#define MyAppExeName "PSADiag.exe"

[Setup]
; GUID unique de l'application : ne jamais le changer entre deux versions.
AppId={{517F86EB-EDB5-4DC1-816A-E5040765C0BE}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\Output
OutputBaseFilename=PSADiag_Setup_{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\{#MyAppExeName}
; SetupIconFile=..\Resources\psadiag.ico

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "Créer un raccourci sur le Bureau"; GroupDescription: "Raccourcis :"

[Files]
; Contenu du dossier Release préparé par windeployqt (exe + DLL Qt + plugins).
Source: "..\Release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Lancer {#MyAppName}"; Flags: nowait postinstall skipifsilent

; Remarque : la base de données, les sauvegardes et les journaux sont stockés dans
; %APPDATA%\PSADiag\PSADiag et ne sont PAS supprimés à la désinstallation
; (l'historique est conservé lors d'une mise à jour).

[Code]
// Sauvegarde de la base avant mise à jour.
procedure CurStepChanged(CurStep: TSetupStep);
var
  DbFile, BackupDir, Target: string;
begin
  if CurStep = ssInstall then
  begin
    DbFile := ExpandConstant('{userappdata}\PSADiag\PSADiag\psadiag.db');
    if FileExists(DbFile) then
    begin
      BackupDir := ExpandConstant('{userappdata}\PSADiag\PSADiag\Backups');
      ForceDirectories(BackupDir);
      Target := BackupDir + '\psadiag_avant_mise_a_jour_' +
                GetDateTimeString('yyyymmdd_hhnnss', #0, #0) + '.db';
      FileCopy(DbFile, Target, False);
    end;
  end;
end;
