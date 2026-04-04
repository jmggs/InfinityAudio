#define MyAppName "InfinityAudio"
#define MyAppVersion "0.3.0"
#define MyAppPublisher "InfinityAudio"
#define MyAppURL "https://github.com/jmggs/InfinityAudio"
#define MyAppExeName "InfinityAudio.exe"
#define MyBuildDir "..\..\build-ucrt"

[Setup]
AppId={{D391A8D1-2AF2-49C1-B0A0-1ABF55AAE102}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=InfinityAudio-setup-{#MyAppVersion}
SetupIconFile=..\icons\infinityaudio.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "{#MyBuildDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: ".ninja_deps,.ninja_log,build.ninja,CMakeCache.txt,cmake_install.cmake,compile_commands.json,CPackConfig.cmake,CPackSourceConfig.cmake,CMakeFiles\*,InfinityAudio_autogen\*,.qt\*"

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent
