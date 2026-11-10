#ifndef ReleaseDir
#define ReleaseDir "cmake-build-release/NeuralNote_artefacts/Release"
#endif

[Setup]
; Identifies the installation across versions; must never change.
AppId={{C01E3C5A-F330-421C-94D7-6264FC93B676}
AppName=NeuralNote
AppVersion=2.1.2
OutputBaseFilename=NeuralNote_Installer_Windows_x64
DefaultDirName={commonpf}\NeuralNote
DisableProgramGroupPage=yes
InfoBeforeFile=..\readme.txt
LicenseFile=..\license.txt
AppPublisher=cleafw (NeuralNote community fork)
AppPublisherURL=https://github.com/cleafw/NeuralNote
AppSupportURL=https://github.com/cleafw/NeuralNote/issues
AppUpdatesURL=https://github.com/cleafw/NeuralNote/releases
AlwaysShowComponentsList=yes
Compression=lzma
SolidCompression=yes
DisableDirPage=no
SetupIconFile=..\..\NeuralNote\Assets\Logo\NeuralNote.ico
UninstallDisplayIcon={app}\NeuralNote.exe
AppCopyright=Copyright (c) 2026 Damien Ronssin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Types]
Name: "custom"; Description: "Custom Installation"; Flags: iscustom

[Components]
Name: "mainapp"; Description: "NeuralNote Standalone"; Types: custom; Flags: disablenouninstallwarning
Name: "plugin"; Description: "NeuralNote VST3"; Types: custom; Flags: disablenouninstallwarning

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: mainapp; Flags: unchecked

[Files]
Source: "..\..\{#ReleaseDir}\Standalone\NeuralNote.exe"; DestDir: "{app}"; Components:mainapp; Flags: ignoreversion recursesubdirs;
Source: "..\..\{#ReleaseDir}\VST3\NeuralNote.vst3\*"; DestDir: "{commoncf}\VST3\NeuralNote.vst3"; Components:plugin; Flags: ignoreversion recursesubdirs;
Source: "..\COMMUNITY_DISTRIBUTION.txt"; DestDir: "{app}\Notices"; Flags: ignoreversion;
Source: "..\AGPL-3.0.txt"; DestDir: "{app}\Notices"; Flags: ignoreversion;
Source: "..\license.txt"; DestDir: "{app}\Notices"; Flags: ignoreversion;

[Icons]
Name: "{autoprograms}\NeuralNote"; Filename: "{app}\NeuralNote.exe"; Components: mainapp
Name: "{autodesktop}\NeuralNote"; Filename: "{app}\NeuralNote.exe"; Tasks: desktopicon

[Code]
// Models, recordings and settings live in the user's profile, not in {app}. Only the profile
// of the user running the uninstaller is cleaned, and a silent uninstall keeps the data.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  DataDir: String;
begin
  if CurUninstallStep <> usPostUninstall then
    Exit;

  DelTree(AddBackslash(GetTempDir) + 'neuralnote', True, True, True);

  DataDir := ExpandConstant('{userappdata}\NeuralNote');
  if DirExists(DataDir) and not UninstallSilent then
    if MsgBox('Also delete NeuralNote''s downloaded models, recordings and settings?' + #13#10#13#10 + DataDir,
              mbConfirmation, MB_YESNO) = IDYES then
      DelTree(DataDir, True, True, True);
end;
