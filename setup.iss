; Inno Setup 脚本 — LSS Video Manager 视频管理客户端
; 使用 Inno Setup 6 编译

#define MyAppName "LSS Video Manager"
#define MyAppNameCN "LSS视频管理客户端"
#define MyAppVersion "1.0"
#define MyAppPublisher "江苏莱瑟斯"
#define MyAppExeName "LSSVideoManager.exe"
#define MyBuildDir "build\Desktop_Qt_6_11_1_MSVC2022_64bit-Release"

[Setup]
AppId={{D9C5E6F7-A8B9-0123-4567-89ABCDEF0123}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\LSS Video Manager
DefaultGroupName=LSS Video Manager
AllowNoIcons=yes
OutputDir=installer
OutputBaseFilename=LSS-Video-Manager-V1.0-Setup
Compression=lzma2/ultra64
SolidCompression=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
PrivilegesRequired=admin

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式:"; Flags: checkedonce

[Files]
; 主程序
Source: "{#MyBuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\LSSVideoManager.exe.manifest"; DestDir: "{app}"; Flags: ignoreversion

; Qt 运行时
Source: "{#MyBuildDir}\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6WebChannel.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6WebEngineCore.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6WebEngineWidgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Qml.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6QmlMeta.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6QmlModels.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6QmlWorkerScript.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Quick.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Quick3DUtils.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6QuickVectorImageGenerator.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6QuickWidgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6OpenGL.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Positioning.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6PrintSupport.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Svg.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6SerialPort.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Lottie.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6LottieVectorImageGenerator.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6VirtualKeyboard.dll"; DestDir: "{app}"; Flags: ignoreversion


; WebEngine 进程
Source: "{#MyBuildDir}\QtWebEngineProcess.exe"; DestDir: "{app}"; Flags: ignoreversion

; FFmpeg 解码
Source: "{#MyBuildDir}\avcodec-62.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\avformat-62.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\avutil-60.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\swscale-9.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\swresample-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\avdevice-62.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\avfilter-11.dll"; DestDir: "{app}"; Flags: ignoreversion

; 系统依赖
Source: "{#MyBuildDir}\d3dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\dxcompiler.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\dxil.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\opengl32sw.dll"; DestDir: "{app}"; Flags: ignoreversion

; Qt 插件目录
Source: "{#MyBuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\position\*"; DestDir: "{app}\position"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\platforminputcontexts\*"; DestDir: "{app}\platforminputcontexts"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\vectorimageformats\*"; DestDir: "{app}\vectorimageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\qmltooling\*"; DestDir: "{app}\qmltooling"; Flags: ignoreversion recursesubdirs createallsubdirs

; resources 目录（QSS、地图 HTML 等内置资源已编译进 exe，但保留目录用于运行期生成文件）
Source: "{#MyBuildDir}\resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs createallsubdirs

; VC++ 运行库（静默安装）
Source: "{#MyBuildDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: ignoreversion deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; 先装 VC++ 运行库，再启动程序
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "正在安装 Visual C++ 运行库..."; Flags: waituntilterminated
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\taskkill"; Parameters: "/f /im LSSVideoManager.exe"; Flags: runhidden

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    CreateDir(ExpandConstant('{app}\snapshots'));
end;
