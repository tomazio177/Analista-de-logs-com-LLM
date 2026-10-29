import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import path from 'node:path';

// Onde estão as ferramentas para compilar a app em C++. O instalador do Qt não as põe no PATH,
// por isso procura-se também onde ele as instala (C:\Qt por omissão; QT_DIR=<pasta> se estiver noutro sítio).
export const WINDOWS = process.platform === 'win32';
export const QT = process.env.QT_DIR ?? (WINDOWS ? 'C:\\Qt' : path.join(process.env.HOME ?? '', 'Qt'));
export const exe = (nome) => (WINDOWS ? `${nome}.exe` : nome);

export function noPath(comando) {
  try {
    const saida = execFileSync(WINDOWS ? 'where' : 'which', [comando], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
    return saida.split(/\r?\n/)[0].trim() || null;
  } catch {
    return null;
  }
}

// Subpastas cujo nome passa no filtro, da versão mais recente para a mais antiga.
export function pastas(pasta, filtro) {
  try {
    return readdirSync(pasta).filter(filtro).sort((a, b) => b.localeCompare(a, 'en', { numeric: true })).map((n) => path.join(pasta, n));
  } catch {
    return [];
  }
}

export const primeiro = (...caminhos) => caminhos.flat().find((c) => c && existsSync(c)) ?? null;

// A primeira linha com texto, ou a primeira que bate com `padrao` (o qmake só diz a versão do Qt na segunda).
export function versao(programa, args, padrao = /./) {
  try {
    const saida = execFileSync(programa, args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], timeout: 15_000 });
    const linhas = saida.split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
    return linhas.find((l) => padrao.test(l)) ?? linhas[0] ?? '';
  } catch {
    return null;
  }
}

// Os programas encontrados (ou null) e, se o Qt veio do instalador oficial, a pasta do kit (C:\Qt\6.12.0\mingw_64),
// que o CMake precisa para encontrar o Qt.
export function localizar() {
  const qmake = primeiro(
    pastas(QT, (n) => /^6\./.test(n)).flatMap((v) => pastas(v, (n) => /^(mingw|msvc|gcc|macos)/.test(n)).map((k) => path.join(k, 'bin', exe('qmake')))),
    noPath('qmake6'), noPath('qmake'),
  );
  const doInstalador = qmake && path.resolve(qmake).startsWith(path.resolve(QT) + path.sep);
  return {
    qmake,
    kit: doInstalador ? path.dirname(path.dirname(qmake)) : null,
    compilador: primeiro(pastas(path.join(QT, 'Tools'), (n) => /^mingw/i.test(n)).map((p) => path.join(p, 'bin', exe('g++'))), noPath('g++'), noPath('clang++')),
    cmake: primeiro(pastas(path.join(QT, 'Tools'), (n) => /^cmake/i.test(n)).map((p) => path.join(p, 'bin', exe('cmake'))), noPath('cmake')),
    ninja: primeiro(path.join(QT, 'Tools', 'Ninja', exe('ninja')), noPath('ninja')),
    creator: primeiro(path.join(QT, 'Tools', 'QtCreator', 'bin', exe('qtcreator')), noPath('qtcreator')),
  };
}
