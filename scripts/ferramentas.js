import { versao, localizar } from './qt.js';

// npm run ferramentas: diz o que já está instalado para fazer a app em C++ e o que falta.
const achados = localizar();

const ferramentas = [
  {
    nome: 'Node.js', caminho: process.execPath, args: ['--version'],
    certo: (v) => { const [a, b] = v.replace('v', '').split('.').map(Number); return a > 22 || (a === 22 && b >= 13); },
    falta: 'Precisa do Node 22.13 ou mais recente (nodejs.org).',
  },
  { nome: 'Git', caminho: undefined, args: ['--version'], falta: 'Instala o Git (git-scm.com).' },
  { nome: 'Qt 6', caminho: achados.qmake, args: ['-version'], linha: /Qt version/, falta: 'Instala o Qt com o instalador oficial.' },
  { nome: 'Compilador C++', caminho: achados.compilador, args: ['--version'], falta: 'Vem com o Qt: no instalador, marca o MinGW.' },
  { nome: 'CMake', caminho: achados.cmake, args: ['--version'], falta: 'Vem com o Qt: no instalador, marca o CMake.' },
  { nome: 'Ninja', caminho: achados.ninja, args: ['--version'], falta: 'Vem com o Qt: no instalador, marca o Ninja.' },
  { nome: 'Qt Creator', caminho: achados.creator, opcional: true, falta: 'Opcional: um editor feito para o Qt. O VS Code também serve.' },
];

console.log('\nO que é preciso para a app em C++:\n');
const faltam = [];
for (const f of ferramentas) {
  const caminho = f.nome === 'Git' ? 'git' : f.caminho;
  // O Qt Creator abre uma janela se o corrermos: basta saber que existe.
  const v = caminho && f.args ? versao(caminho, f.args, f.linha) : caminho && 'encontrado';
  const ok = v != null && (!f.certo || f.certo(v));
  const marca = ok ? '✓' : f.opcional ? '·' : '✗';
  console.log(`  ${marca} ${f.nome.padEnd(16)} ${ok ? v : f.falta}`);
  if (ok && caminho !== process.execPath && caminho !== 'git') console.log(`    ${''.padEnd(16)} ${caminho}`);
  if (!ok && !f.opcional) faltam.push(f.nome);
}
console.log(faltam.length ? `\nFalta: ${faltam.join(', ')}.\n` : '\nEstá tudo. Podemos começar a app.\n');
