#ifndef BundlePath
  #define BundlePath "..\build\DISRUPTION_artefacts\Release\VST3\DISRUPTION.vst3"
#endif
#ifndef PackageDocs
  #define PackageDocs "..\release\package-docs"
#endif
#ifndef OutputPath
  #define OutputPath "..\release"
#endif

[Setup]
AppId={{7EF22018-91AB-48AC-873F-A2CA30D18019}
AppName=VoxxLab DISRUPTION
AppVersion=1.0.0
AppVerName=VoxxLab DISRUPTION 1.0.0
AppPublisher=VoxxLab
AppPublisherURL=https://github.com/scryingorb/DISRUPTION
AppSupportURL=https://github.com/scryingorb/DISRUPTION/issues
DefaultDirName={autopf}\VoxxLab\DISRUPTION
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputPath}
OutputBaseFilename=DISRUPTION-1.0.0-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=VoxxLab DISRUPTION
VersionInfoVersion=1.0.0.0
CloseApplications=yes
RestartApplications=no

[Files]
Source: "{#BundlePath}\*"; DestDir: "{commoncf64}\VST3\DISRUPTION.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageDocs}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
; Only remove empty directories; user presets are deliberately outside these paths.
Type: dirifempty; Name: "{commoncf64}\VST3\DISRUPTION.vst3\Contents\Resources"
Type: dirifempty; Name: "{commoncf64}\VST3\DISRUPTION.vst3\Contents\x86_64-win"
Type: dirifempty; Name: "{commoncf64}\VST3\DISRUPTION.vst3\Contents"
Type: dirifempty; Name: "{commoncf64}\VST3\DISRUPTION.vst3"
