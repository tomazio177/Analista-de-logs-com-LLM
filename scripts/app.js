import { execFileSync, spawn } from 'node:child_process';
import path from 'node:path';
import { exe } from './qt.js';
import { compilarApp, FaltaFerramenta } from './qt-compilar.js';

// npm run app               compila a app e abre-a
// npm run app -- --testes   compila e corre os testes da app, sem abrir janelas
const projeto = path.resolve(import.meta.dirname, '..');
const build = path.join(projeto, 'app', 'build');

let fase = 'A compilação';
try {
  const f = compilarApp(build);
  if (process.argv.includes('--testes')) {
    fase = 'Um dos testes';
    execFileSync(path.join(path.dirname(f.cmake), exe('ctest')), ['--test-dir', build, '--output-on-failure'], { stdio: 'inherit', env: f.env });
  } else {
    // Solta a app: este comando pode fechar, a janela continua.
    spawn(path.join(build, exe('analista')), [], { env: { ...f.env, ANALISTA_MOTOR: projeto }, detached: true, stdio: 'ignore' }).unref();
    console.log('\nA app abriu numa janela própria.\n');
  }
} catch (erro) {
  console.error(erro instanceof FaltaFerramenta ? `\n${erro.message}\n` : `\n${fase} falhou. Cola-me a mensagem acima.\n`);
  process.exit(1);
}
