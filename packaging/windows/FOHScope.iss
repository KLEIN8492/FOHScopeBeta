; FOHScope Windows installer (Inno Setup 6). Built by scripts/package-windows.ps1:
;   ISCC /DAppVersion=0.2.0 /DSourceExe=<path to FOHScope.exe> /DOutputDir=dist FOHScope.iss
; Per-user install: no admin rights needed, uninstaller in Settings > Apps.

#ifndef AppVersion
  #error AppVersion not defined
#endif

[Setup]
AppId={{7C1F3E52-0A44-4A2E-9B57-5E3F0B6C9D21}
AppName=FOHScope
AppVersion={#AppVersion}
AppPublisher=FOHScope
AppPublisherURL=https://github.com/KLEIN8492/FOHScopeBeta
DefaultDirName={localappdata}\Programs\FOHScope
DefaultGroupName=FOHScope
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=FOHScope-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\Resources\AppIcon.ico
UninstallDisplayIcon={app}\FOHScope.exe
LicenseFile=..\..\LICENSE
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"
Source: "..\..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"
Source: "..\..\Resources\fonts\OFL.txt"; DestDir: "{app}"

[Icons]
Name: "{group}\FOHScope"; Filename: "{app}\FOHScope.exe"
Name: "{group}\Uninstall FOHScope"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\FOHScope.exe"; Description: "Start FOHScope"; Flags: nowait postinstall skipifsilent
