#ifndef ApplicationVersion
#define ApplicationVersion "unknown"
#endif

[Setup]
AppName=alice-tools
AppVersion={#ApplicationVersion}
DefaultDirName={autopf}\alice-tools
DefaultGroupName=alice-tools
ChangesAssociations=yes
ChangesEnvironment=true
PrivilegesRequiredOverridesAllowed=dialog
OutputBaseFilename=alice-tools-windows-x86_64-{#ApplicationVersion}

[Files]
Source: "..\alice-tools\alice.exe"; DestDir: "{app}"
Source: "..\alice-tools\galice\*"; DestDir: "{app}"; Flags: recursesubdirs
Source: "..\alice-tools\licenses\*"; DestDir: "{app}\licenses"; Flags: recursesubdirs
Source: "..\alice-tools\*.md"; DestDir: "{app}"
Source: "_alice-ar-extract.bat"; DestDir: "{app}"
Source: "icon.ico"; DestDir: "{app}"

[Tasks]
name: startmenu; Description: "Create shortcut in Start Menu"
name: modifypath; Description: "Add application directory to PATH environment variable"; Flags: unchecked
name: contextmenus; Description: "Create context menu enties in file explorer"
name: associate_arc; Description: "Associate archive files (.aar, .afa, .ald, .alk, .dlf)"
name: associate_acx; Description: "Associate .acx files"
Name: associate_ain; Description: "Associate .ain files"
Name: associate_ex; Description: "Associate .ex files"
Name: associate_flat; Description: "Associate .flat files"
Name: associate_img; Description: "Associate image files (.ajp, .dcf, .pcf, .pms, .qnt, .rou)"
Name: associate_manifest; Description: "Associate manifest files (.alicepack, .batchpack)"
Name: associate_pje; Description: "Associate .pje files"

[Icons]
Name: "{group}\GAlice"; Filename: "{app}\galice.exe"; WorkingDir: "C:\"; Tasks: startmenu
Name: "{group}\Uninstall alice-tools"; Filename: "{uninstallexe}"; Tasks: startmenu

[Registry]
; .ALICEPACK
Root: HKA; Subkey: "Software\Classes\.alicepack\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.alicepack"; ValueData: ""; Flags: uninsdeletevalue; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alicepack"; ValueType: string; ValueName: ""; ValueData: "alice-tools ALICEPACK manifest"; Flags: uninsdeletekey; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alicepack\shell\Pack"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alicepack\shell\Pack\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" ar pack ""%1"""; Tasks: contextmenus
; .BATCHPACK
Root: HKA; Subkey: "Software\Classes\.batchpack\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.batchpack"; ValueData: ""; Flags: uninsdeletevalue; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.batchpack"; ValueType: string; ValueName: ""; ValueData: "alice-tools BATCHPACK manifest"; Flags: uninsdeletekey; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.batchpack\shell\Pack"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.batchpack\shell\Pack\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" ar pack ""%1"""; Tasks: contextmenus
; .PJE
Root: HKA; Subkey: "Software\Classes\.pje\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.pje"; ValueData: ""; Flags: uninsdeletevalue; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pje"; ValueType: string; ValueName: ""; ValueData: "alice-tools project file"; Flags: uninsdeletekey; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pje\shell\Build"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pje\shell\Build\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" project build ""%1"""; Tasks: contextmenus
; .AAR
Root: HKA; Subkey: "Software\Classes\.aar\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.aar"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.aar"; ValueType: string; ValueName: ""; ValueData: "AliceSoft AAR archive"; Flags: uninsdeletekey; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.aar\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_arc
Root: HKA; Subkey: "Software\Classes\AliceFile.aar\shell\Extract"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.aar\shell\Extract"; ValueType: string; ValueName: "subcommands"; ValueData: ""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.aar\shell\Extract\shell\Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.aar\shell\Extract\shell\Extract (raw)\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --raw"; Tasks: contextmenus
; .AFA
Root: HKA; Subkey: "Software\Classes\.afa\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.afa"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa"; ValueType: string; ValueName: ""; ValueData: "AliceSoft AFA archive"; Flags: uninsdeletekey; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_arc
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\Extract"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\Extract"; ValueType: string; ValueName: "subcommands"; ValueData: ""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\Extract\shell\Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\Extract\shell\Extract (raw)\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --raw"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\Extract\shell\Extract with manifest\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --manifest ""%1.alicepack"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.afa\shell\Extract\shell\Extract (raw) with manifest\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --raw --manifest ""%1.alicepack"""; Tasks: contextmenus
; .ALD
Root: HKA; Subkey: "Software\Classes\.ald\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.ald"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ald"; ValueType: string; ValueName: ""; ValueData: "AliceSoft ALD archive"; Flags: uninsdeletekey; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ald\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_arc
Root: HKA; Subkey: "Software\Classes\AliceFile.ald\shell\Extract"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ald\shell\Extract"; ValueType: string; ValueName: "subcommands"; ValueData: ""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ald\shell\Extract\shell\Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ald\shell\Extract\shell\Extract (raw)\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --raw"; Tasks: contextmenus
; .ALK
Root: HKA; Subkey: "Software\Classes\.alk\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.alk"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alk"; ValueType: string; ValueName: ""; ValueData: "AliceSoft ALK archive"; Flags: uninsdeletekey; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alk\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_arc
Root: HKA; Subkey: "Software\Classes\AliceFile.alk\shell\Extract"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alk\shell\Extract"; ValueType: string; ValueName: "subcommands"; ValueData: ""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alk\shell\Extract\shell\Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.alk\shell\Extract\shell\Extract (raw)\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --raw"; Tasks: contextmenus
; .DLF
Root: HKA; Subkey: "Software\Classes\.dlf\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.dlf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dlf"; ValueType: string; ValueName: ""; ValueData: "AliceSoft DLF archive"; Flags: uninsdeletekey; Tasks: associate_arc or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dlf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_arc
Root: HKA; Subkey: "Software\Classes\AliceFile.dlf\shell\Extract"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dlf\shell\Extract"; ValueType: string; ValueName: "subcommands"; ValueData: ""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dlf\shell\Extract\shell\Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dlf\shell\Extract\shell\Extract (raw)\command"; ValueType: string; ValueName: ""; ValueData: """{app}\_alice-ar-extract.bat"" ""%1"" --raw"; Tasks: contextmenus
; .AIN
Root: HKA; Subkey: "Software\Classes\.ain\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.ain"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_ain or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain"; ValueType: string; ValueName: ""; ValueData: "AliceSoft script file"; Flags: uninsdeletekey; Tasks: associate_ain or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_ain
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Dump"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Dump"; ValueType: string; ValueName: "subcommands"; ValueData: ""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Dump\shell\Dump JAM\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" ain dump -c -o ""%1.jam"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Dump\shell\Dump JSON\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" ain dump --json -o ""%1.json"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Dump\shell\Dump text\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" ain dump -t -o ""%1.txt"" ""%1"""; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Initialize mod project"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ain\shell\Initialize mod project\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" project init --mod -o mod ""%1"""; Tasks: contextmenus
; .ACX
Root: HKA; Subkey: "Software\Classes\.acx\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.acx"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_acx or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.acx"; ValueType: string; ValueName: ""; ValueData: "AliceSoft ACX file"; Flags: uninsdeletekey; Tasks: associate_acx or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.acx\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_acx
Root: HKA; Subkey: "Software\Classes\AliceFile.acx\shell\Decompile"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.acx\shell\Decompile\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" acx dump -o ""%1.csv"" ""%1"""; Tasks: contextmenus
; .EX
Root: HKA; Subkey: "Software\Classes\.ex\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.ex"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_ex or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ex"; ValueType: string; ValueName: ""; ValueData: "AliceSoft EX file"; Flags: uninsdeletekey; Tasks: associate_ex or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ex\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_ex
Root: HKA; Subkey: "Software\Classes\AliceFile.ex\shell\Decompile"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ex\shell\Decompile\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" ex dump -o ""%1.x"" ""%1"""; Tasks: contextmenus
; .FLAT
Root: HKA; Subkey: "Software\Classes\.flat\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.flat"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_flat or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.flat"; ValueType: string; ValueName: ""; ValueData: "AliceSoft FLAT file"; Flags: uninsdeletekey; Tasks: associate_flat
Root: HKA; Subkey: "Software\Classes\AliceFile.flat\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_flat
Root: HKA; Subkey: "Software\Classes\AliceFile.flat\shell\Extract"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.flat\shell\Extract\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" flat extract -o ""%1.x"" ""%1"""; Tasks: contextmenus
; .AJP
Root: HKA; Subkey: "Software\Classes\.ajp\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.ajp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ajp"; ValueType: string; ValueName: ""; ValueData: "AliceSoft AJP image"; Flags: uninsdeletekey; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ajp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_img
Root: HKA; Subkey: "Software\Classes\AliceFile.ajp\shell\Convert to PNG"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.ajp\shell\Convert to PNG\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" cg convert -t png ""%1"""; Tasks: contextmenus
; .DCF
Root: HKA; Subkey: "Software\Classes\.dcf\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.dcf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dcf"; ValueType: string; ValueName: ""; ValueData: "AliceSoft DCF image"; Flags: uninsdeletekey; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dcf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_img
Root: HKA; Subkey: "Software\Classes\AliceFile.dcf\shell\Convert to PNG"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.dcf\shell\Convert to PNG\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" cg convert -t png ""%1"""; Tasks: contextmenus
; .PCF
Root: HKA; Subkey: "Software\Classes\.pcf\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.pcf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pcf"; ValueType: string; ValueName: ""; ValueData: "AliceSoft PCF image"; Flags: uninsdeletekey; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pcf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_img
Root: HKA; Subkey: "Software\Classes\AliceFile.pcf\shell\Convert to PNG"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pcf\shell\Convert to PNG\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" cg convert -t png ""%1"""; Tasks: contextmenus
; .PMS
Root: HKA; Subkey: "Software\Classes\.pms\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.pms"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pms"; ValueType: string; ValueName: ""; ValueData: "AliceSoft PMS image"; Flags: uninsdeletekey; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pms\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_img
Root: HKA; Subkey: "Software\Classes\AliceFile.pms\shell\Convert to PNG"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.pms\shell\Convert to PNG\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" cg convert -t png ""%1"""; Tasks: contextmenus
; .QNT
Root: HKA; Subkey: "Software\Classes\.qnt\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.qnt"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.qnt"; ValueType: string; ValueName: ""; ValueData: "AliceSoft QNT image"; Flags: uninsdeletekey; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.qnt\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_img
Root: HKA; Subkey: "Software\Classes\AliceFile.qnt\shell\Convert to PNG"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.qnt\shell\Convert to PNG\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" cg convert -t png ""%1"""; Tasks: contextmenus
; .ROU
Root: HKA; Subkey: "Software\Classes\.rou\OpenWithProgids"; ValueType: string; ValueName: "AliceFile.rou"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.rou"; ValueType: string; ValueName: ""; ValueData: "AliceSoft ROU image"; Flags: uninsdeletekey; Tasks: associate_img or contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.rou\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\galice.exe"" ""%1"""; Tasks: associate_img
Root: HKA; Subkey: "Software\Classes\AliceFile.rou\shell\Convert to PNG"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\icon.ico"; Tasks: contextmenus
Root: HKA; Subkey: "Software\Classes\AliceFile.rou\shell\Convert to PNG\command"; ValueType: string; ValueName: ""; ValueData: """{app}\alice.exe"" cg convert -t png ""%1"""; Tasks: contextmenus

[Code]
const
    ModPathName = 'modifypath';
    ModPathType = 'user';

function ModPathDir(): TArrayOfString;
begin
    setArrayLength(Result, 1)
    Result[0] := ExpandConstant('{app}');
end;
#include "modpath.iss"
