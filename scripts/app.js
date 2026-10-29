import { execFileSync, spawn } from 'node:child_process';
import path from 'node:path';
import { WINDOWS, exe, localizar } from './qt.js';

// npm run app            compila a app e abre-a
// npm run app -- --testes   compila e corre os testes da app, sem abrir janelas
const projeto = path.resolve(import.meta.dirname, '..');
const fonte = path.join(projeto, 'app');
const build = path.join(fonte, 'build');
const f = localizar();

const falta = [['Qt 6', f.qmake], ['compilador C++', f.compilador], ['CMake', f.cmake], ['Ninja', f.ninja]].filter(([, c]) => !c).map(([n]) => n);
if (falta.length) {
  console.error(`\nFalta instalar: ${falta.join(', ')}. Corre: npm run ferramentas\n`);
  process.exit(1);
}

// O compilador e o Qt precisam das suas pastas no PATH, tanto para compilar como para a app arrancar (as DLL do Qt).
const pastasDeTrabalho = [path.dirname(f.compilador), path.dirname(f.ninja), path.dirname(f.qmake), path.dirname(f.cmake)];
const env = { ...process.env, PATH: [...pastasDeTrabalho, process.env.PATH].join(path.delimiter) };
const correr = (programa, args) => execFileSync(programa, args, { stdio: 'inherit', env });

try {
  correr(f.cmake, [
    '-S', fonte, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    `-DCMAKE_MAKE_PROGRAM=${f.ninja}`, `-DCMAKE_CXX_COMPILER=${f.compilador}`,
    ...(f.kit ? [`-DCMAKE_PREFIX_PATH=${f.kit}`] : []),
  ]);
  correr(f.cmake, ['--build', build]);
  if (process.argv.includes('--testes')) {
    correr(path.join(path.dirname(f.cmake), exe('ctest')), ['--test-dir', build, '--output-on-failure']);
  } else {
    // Solta a app: este comando pode fechar, a janela continua.
    spawn(path.join(build, exe('analista')), [], { env: { ...env, ANALISTA_MOTOR: projeto }, detached: true, stdio: 'ignore' }).unref();
    console.log('\nA app abriu numa janela própria.\n');
  }
} catch {
  console.error('\nA compilação falhou. Cola-me a mensagem de erro acima.\n');
  process.exit(1);
}
