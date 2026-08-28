#define MyAppName "EchoBox II"
#ifndef MyAppVersion
  #define MyAppVersion "4.2.0"
#endif
#define MyAppPublisher "BANANCHIKIREAL"
#define MyAppURL "https://github.com/BANANCHIKIREAL/EchoBox-II"
#define MyAppExeName "EchoBoxII.exe"

[Setup]
AppId={{32B2150A-F1D0-424F-9FC8-7214F27C9434}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\EchoBox II
DefaultGroupName=EchoBox II
DisableProgramGroupPage=yes
; Ставится в пользовательскую папку без прав администратора —
; UAC не нужен ни на установке, ни на автообновлении
PrivilegesRequired=lowest
OutputDir=..\installer_out
OutputBaseFilename=EchoBoxII-Setup-{#MyAppVersion}
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\EchoBox-II\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\EchoBox II"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,EchoBox II}"; Filename: "{uninstallexe}"
; Всегда пересоздаём ярлык при установке и обновлении. Это чинит старый
; ярлык, даже если каталог приложения когда-либо изменился.
Name: "{autodesktop}\EchoBox II"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,EchoBox II}"; Flags: nowait postinstall skipifsilent
; Тихая установка запускается только безопасным встроенным updater-ом после
; проверки SHA-256. После обновления возвращаем приложение без PowerShell.
Filename: "{app}\{#MyAppExeName}"; Flags: nowait skipifnotsilent
