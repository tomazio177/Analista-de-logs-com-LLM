import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import path from 'node:path';

// npm run ferramentas: diz o que já está instalado para fazer a app em C++ e o que falta.
// O instalador do Qt não põe o compilador nem o CMake no PATH, por isso também se procura onde ele os põe
// (C:\Qt por omissão; QT_DIR=<pasta> se o instalaste noutro sítio).
const WINDOWS = process.platform === 'win32';
const QT = process.env.QT_DIR ?? (WINDOWS ? 'C:\\Qt' : path.join(process.env.HOME ?? '', 'Qt'));
const exe = (nome) => (WINDOWS ? `${nome}.exe` : nome);

function noPath(comando) {
  try {
    const saida = execFileSync(WINDOWS ? 'where' : 'which', [comando], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
    return saida.split(/\r?\n/)[0].trim() || null;
  } catch {
    return null;
  }
}

// Subpastas cujo nome passa no filtro, da versão mais recente para a mais antiga.
function pastas(pasta, filtro) {
  try {
    return readdirSync(pasta).filter(filtro).sort((a, b) => b.localeCompare(a, 'en', { numeric: true })).map((n) => path.join(pasta, n));
  } catch {
    return [];
  }
}

const primeiro = (...caminhos) => caminhos.flat().find((c) => c && existsSync(c)) ?? null;

// A primeira linha com texto, ou a primeira que bate com `padrao` (o qmake só diz a versão do Qt na segunda).
function versao(programa, args, padrao = /./) {
  try {
    const saida = execFileSync(programa, args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], timeout: 15_000 });
    const linhas = saida.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
    return linhas.find((l) => padrao.test(l)) ?? linhas[0] ?? '';
  } catch {
    return null;
  }
}

const ferramentas = [
  {
    nome: 'Node.js', onde: () => process.execPath, args: ['--version'],
    certo: (v) => { const [a, b] = v.replace('v', '').split('.').map(Number); return a > 22 || (a === 22 && b >= 13); },
    falta: 'Precisa do Node 22.13 ou mais recente (nodejs.org).',
  },
  { nome: 'Git', onde: () => noPath('git'), args: ['--version'], falta: 'Instala o Git (git-scm.com).' },
  {
    nome: 'Qt 6',
    onde: () => primeiro(
      pastas(QT, (n) => /^6\./.test(n)).flatMap((v) => pastas(v, (n) => /^(mingw|msvc|gcc|macos)/.test(n)).map((k) => path.join(k, 'bin', exe('qmake')))),
      noPath('qmake6'), noPath('qmake'),
    ),
    args: ['-version'], linha: /Qt version/, falta: 'Instala o Qt com o instalador oficial.',
  },
  {
    nome: 'Compilador C++',
    onde: () => primeiro(pastas(path.join(QT, 'Tools'), (n) => /^mingw/i.test(n)).map((p) => path.join(p, 'bin', exe('g++'))), noPath('g++'), noPath('clang++')),
    args: ['--version'], falta: 'Vem com o Qt: no instalador, marca o MinGW.',
  },
  {
    nome: 'CMake', onde: () => primeiro(pastas(path.join(QT, 'Tools'), (n) => /^cmake/i.test(n)).map((p) => path.join(p, 'bin', exe('cmake'))), noPath('cmake')),
    args: ['--version'], falta: 'Vem com o Qt: no instalador, marca o CMake.',
  },
  {
    nome: 'Ninja', onde: () => primeiro(path.join(QT, 'Tools', 'Ninja', exe('ninja')), noPath('ninja')),
    args: ['--version'], falta: 'Vem com o Qt: no instalador, marca o Ninja.',
  },
  {
    nome: 'Qt Creator', opcional: true,
    onde: () => primeiro(path.join(QT, 'Tools', 'QtCreator', 'bin', exe('qtcreator')), noPath('qtcreator')),
    falta: 'Opcional: um editor feito para o Qt. O VS Code também serve.',
  },
];

console.log('\nO que é preciso para a app em C++:\n');
const faltam = [];
for (const f of ferramentas) {
  const caminho = f.onde();
  // O Qt Creator abre uma janela se o corrermos: basta saber que existe.
  const v = caminho && f.args ? versao(caminho, f.args, f.linha) : caminho && 'encontrado';
  const ok = v != null && (!f.certo || f.certo(v));
  const marca = ok ? '✓' : f.opcional ? '·' : '✗';
  console.log(`  ${marca} ${f.nome.padEnd(16)} ${ok ? v : f.falta}`);
  if (ok && caminho !== process.execPath) console.log(`    ${''.padEnd(16)} ${caminho}`);
  if (!ok && !f.opcional) faltam.push(f.nome);
}
console.log(faltam.length ? `\nFalta: ${faltam.join(', ')}.\n` : '\nEstá tudo. Podemos começar a app.\n');
