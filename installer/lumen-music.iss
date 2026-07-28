; Inno Setup script for Lumen Music.
;
; Prerequisites:
;   1. Build Release and install into dist\LumenMusic\
;        cmake --preset msvc-ninja
;        cmake --build --preset msvc-ninja
;        cmake --install build/msvc-ninja --prefix dist/LumenMusic
;   2. Compile this script:
;        ISCC.exe installer\lumen-music.iss
;
; Output:
;   dist\LumenMusic-v{VERSION}-setup.exe
;   where VERSION is read from the built exe VERSIONINFO (CMake PROJECT_VERSION).
;
; AppId is pinned so future renames of AppName do not create duplicate
; uninstall entries (Task.md P7).

#define MyAppName "Lumen Music"
#define MyAppPublisher "Lumen Connection"
#define MyAppExeName "LumenMusic.exe"
#define MyAppSource "..\dist\LumenMusic"
; Single source of truth: VERSIONINFO embedded by CMake from PROJECT_VERSION.
#define MyAppVersion GetVersionNumbersString(MyAppSource + "\" + MyAppExeName)
; Stable product GUID — do not change after the first public v2.0.0 installer.
#define MyAppId "{{8F3A2C1B-6E4D-4A9F-B2C7-5D1E0F9A8B3C}"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\Lumen Music
DefaultGroupName=Lumen Music
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\resources\icon.ico
OutputDir=..\dist
OutputBaseFilename=LumenMusic-v{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=lowest
DisableProgramGroupPage=no

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Entire deployed tree (exe + Qt DLLs + plugins + yt-dlp).
Source: "{#MyAppSource}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
