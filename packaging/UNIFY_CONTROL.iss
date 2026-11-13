#define AppName "UNIFY CONTROL"
#define AppVersion "1.8.0"
#define AppPublisher "UNIFY CONTROL"
#define AppExeName "UNIFY_CONTROL.exe"

[Setup]
AppId={{A9C6D7B4-3E11-4D43-9C42-1C0B7B1A1700}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\UNIFY CONTROL
DefaultGroupName={#AppName}
OutputDir=..\dist
OutputBaseFilename=UNIFY_CONTROL_Setup_{#AppVersion}_x64
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
SetupIconFile=..\Assets\UNIFY_CONTROL.ico
UninstallDisplayIcon={app}\UNIFY_CONTROL.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
Source: "..\dist\UNIFY_CONTROL_RELEASE\UNIFY_CONTROL.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\UNIFY_CONTROL_RELEASE\frontend\*"; DestDir: "{app}\frontend"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\UNIFY_CONTROL_RELEASE\database\*"; DestDir: "{app}\database"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\UNIFY_CONTROL_RELEASE\UNIFY_CONTROL.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\UNIFY_CONTROL_RELEASE\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\UNIFY CONTROL"; Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall UNIFY CONTROL"; Filename: "{uninstallexe}"
Name: "{autodesktop}\UNIFY CONTROL"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\MicrosoftEdgeWebView2RuntimeInstallerX64.exe"; Parameters: "/silent /install"; StatusMsg: "Installing or updating Microsoft Edge WebView2 Runtime..."; Flags: waituntilterminated
Filename: "{app}\{#AppExeName}"; Description: "Launch UNIFY CONTROL"; Flags: postinstall nowait skipifsilent

