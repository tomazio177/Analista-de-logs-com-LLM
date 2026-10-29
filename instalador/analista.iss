; Instalador do Analista de logs (Inno Setup 6.3 ou mais recente).
; Normalmente corre-se pelo npm run empacotar, que passa a pasta e a versão.
#ifndef Fonte
  #define Fonte "..\dist\Analista de logs"
#endif
#ifndef Versao
  #define Versao "0.1.0"
#endif

[Setup]
AppId={{8C1B2F4E-5A7D-4E1B-9C3A-6F2D1E0B7A44}
AppName=Analista de logs
AppVersion={#Versao}
AppPublisher=Analista de logs
; Em Program Files só o administrador escreve. Importa: a app corre o leitor de logins como administrador,
; e esse código não pode estar numa pasta onde qualquer programa o possa alterar.
DefaultDirName={autopf}\Analista de logs
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=Analista-de-logs-{#Versao}-instalador
SetupIconFile=..\app\icone\analista.ico
UninstallDisplayIcon={app}\analista.ico
WizardStyle=modern
Compression=lzma2
SolidCompression=yes

[Languages]
Name: "portugues"; MessagesFile: "compiler:Languages\Portuguese.isl"

[Tasks]
Name: "atalho"; Description: "Criar um atalho no ambiente de trabalho"; GroupDescription: "Atalhos:"; Flags: unchecked

[Files]
Source: "{#Fonte}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Analista de logs"; Filename: "{app}\analista.exe"; IconFilename: "{app}\analista.ico"
Name: "{autodesktop}\Analista de logs"; Filename: "{app}\analista.exe"; IconFilename: "{app}\analista.ico"; Tasks: atalho

[Run]
; Abre a app no fim, como o utilizador normal e não como administrador (o instalador corre como administrador).
Filename: "{app}\analista.exe"; Description: "Abrir o Analista de logs"; Flags: nowait postinstall skipifsilent runasoriginaluser

; Ao desinstalar, os dados (histórico e chave da API, em %APPDATA%\Analista de logs) ficam: são do utilizador.
