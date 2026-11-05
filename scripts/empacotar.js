import { cpSync, existsSync, mkdirSync, readFileSync, rmSync, statSync, readdirSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { WINDOWS, exe, noPath, primeiro } from './qt.js';
import { compilarApp, FaltaFerramenta } from './qt-compilar.js';

// npm run empacotar: junta numa pasta tudo o que a app precisa para correr num PC sem Node nem Qt,
// e, se o Inno Setup estiver instalado, faz o instalador.
//
//   dist/Analista de logs/
//     analista.exe + DLL do Qt e do compilador (windeployqt)
//     analista.ico
//     node/node.exe          o mesmo Node que corre este comando
//     motor/                 package.json, src/ e node_modules/ (sem testes, sem dados, sem .env)
//     LICENSE.txt, licencas/ a licença da app e as dos outros (o Qt, o Node e as bibliotecas): distribuí-los obriga a isso
const projeto = path.resolve(import.meta.dirname, '..');
const versao = JSON.parse(readFileSync(path.join(projeto, 'package.json'), 'utf8')).version;
const build = path.join(projeto, 'app', 'build-release');
const destino = path.join(projeto, 'dist', 'Analista de logs');

let f;
try {
  f = compilarApp(build);
} catch (erro) {
  console.error(erro instanceof FaltaFerramenta ? `\n${erro.message}\n` : '\nA compilação falhou. Cola-me a mensagem acima.\n');
  process.exit(1);
}

console.log(`\nA juntar tudo em ${destino}…`);
rmSync(destino, { recursive: true, force: true });
mkdirSync(destino, { recursive: true });

cpSync(path.join(build, exe('analista')), path.join(destino, exe('analista')));
cpSync(path.join(projeto, 'app', 'icone', 'analista.ico'), path.join(destino, 'analista.ico'));

// As DLL do Qt e do compilador, e os plugins (a plataforma Windows, os estilos).
const windeployqt = primeiro(path.join(path.dirname(f.qmake), exe('windeployqt')), noPath('windeployqt'));
if (windeployqt) {
  execFileSync(windeployqt, ['--release', '--compiler-runtime', '--no-translations', '--no-system-d3d-compiler', '--no-opengl-sw', path.join(destino, exe('analista'))],
    { stdio: 'inherit', env: f.env });
} else {
  console.log(WINDOWS ? '⚠ Não encontrei o windeployqt: faltam as DLL do Qt na pasta.' : '(Fora do Windows não há windeployqt: a pasta usa o Qt do sistema.)');
}

// O Node: o mesmo executável que está a correr isto. No Windows é um só ficheiro, que funciona sozinho.
mkdirSync(path.join(destino, 'node'));
cpSync(process.execPath, path.join(destino, 'node', exe('node')));
// A licença do Node que vai é a do Node que vai: ao lado do executável (Windows) ou uma pasta acima (Linux, macOS).
// Se não houver, fica a cópia guardada no projeto.
const licencaDoNode = primeiro(path.join(path.dirname(process.execPath), 'LICENSE'), path.join(path.dirname(process.execPath), '..', 'LICENSE'))
  ?? path.join(projeto, 'instalador', 'licencas', 'node-LICENSE.txt');
cpSync(licencaDoNode, path.join(destino, 'node', 'LICENSE'));

// A licença da app e os avisos dos outros. As das bibliotecas do motor já vão dentro de motor/node_modules.
cpSync(path.join(projeto, 'LICENSE'), path.join(destino, 'LICENSE.txt'));
mkdirSync(path.join(destino, 'licencas'));
for (const f of ['AVISOS.txt', 'LGPL-3.txt', 'GPL-3.txt']) cpSync(path.join(projeto, 'instalador', 'licencas', f), path.join(destino, 'licencas', f));
// As fontes vão dentro do analista.exe; a licença delas (OFL) vai ao lado.
for (const f of ['OFL-Geist.txt', 'OFL-JetBrainsMono.txt']) cpSync(path.join(projeto, 'app', 'fontes', f), path.join(destino, 'licencas', f));

// O motor, só com o que corre: nada de testes, dados, chaves ou ficheiros de desenvolvimento.
const motor = path.join(destino, 'motor');
mkdirSync(motor);
cpSync(path.join(projeto, 'package.json'), path.join(motor, 'package.json'));
cpSync(path.join(projeto, 'src'), path.join(motor, 'src'), { recursive: true });
cpSync(path.join(projeto, 'node_modules'), path.join(motor, 'node_modules'), { recursive: true });

const tamanho = (p) => (statSync(p).isDirectory() ? readdirSync(p).reduce((t, n) => t + tamanho(path.join(p, n)), 0) : statSync(p).size);
console.log(`\nPasta pronta: ${destino} (${Math.round(tamanho(destino) / 1e6)} MB)`);
console.log('Funciona sozinha: copia-a para onde quiseres e abre o analista.exe.');

// O instalador, se houver Inno Setup.
const iscc = WINDOWS
  ? primeiro(
      path.join(process.env['ProgramFiles(x86)'] ?? 'C:\\Program Files (x86)', 'Inno Setup 6', 'ISCC.exe'),
      path.join(process.env.ProgramFiles ?? 'C:\\Program Files', 'Inno Setup 6', 'ISCC.exe'),
      path.join(process.env.LOCALAPPDATA ?? '', 'Programs', 'Inno Setup 6', 'ISCC.exe'),
      noPath('iscc'),
    )
  : null;
if (iscc) {
  execFileSync(iscc, [`/DFonte=${destino}`, `/DVersao=${versao}`, path.join(projeto, 'instalador', 'analista.iss')], { stdio: 'inherit' });
  console.log(`\nInstalador pronto em ${path.join(projeto, 'dist')}.\n`);
} else if (WINDOWS) {
  console.log('\nPara fazer também o instalador, instala o Inno Setup (winget install JRSoftware.InnoSetup) e corre outra vez.\n');
} else {
  console.log();
}
if (!existsSync(path.join(motor, 'src', 'vigiar.js'))) process.exit(1);
