; installer_windows.iss
; 請使用 Inno Setup Compiler 開啟此檔案並編譯

#define MyAppName "SmartFile"
#define MyAppVersion "1.0"
#define MyAppPublisher "SmartFile Team"
#define MyAppExeName "SmartFileOrganizer.exe"
#define SourceDir "release_staging"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{A00AF728-5E7D-40C5-9C8E-11B9F624BA45}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
; 允許使用者建立桌面捷徑
AllowNoIcons=yes
; 輸出安裝檔名稱與位置
OutputBaseFilename=SmartFile_Setup_v{#MyAppVersion}
OutputDir=.
; 壓縮設定
Compression=lzma
SolidCompression=yes
; 讓安裝檔看起來更現代
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
; 若需要繁體中文，可從 Inno Setup 下載 ChineseTraditional.isl 並指定路徑
; Name: "chinesetraditional"; MessagesFile: "compiler:Languages\ChineseTraditional.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 複製 release_staging 資料夾下的所有內容
; Flags: ignoreversion 確保升級時覆蓋舊檔
; recursesubdirs 包含子資料夾 (如 models, platforms)
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; 安裝完成後讓使用者立即執行
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
// 若有需要檢查 VC++ Redistributable 是否安裝，可在這裡加入程式碼
