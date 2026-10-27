import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { abrir } from '../src/historico.js';

// Cada teste trabalha numa base de dados própria, numa pasta temporária que desaparece no fim.
// No Windows um ficheiro aberto não se apaga, por isso as bases abertas pelo teste são fechadas antes.
export async function comBaseTemporaria(fn) {
  const pasta = mkdtempSync(path.join(tmpdir(), 'analista-'));
  const ficheiro = path.join(pasta, 'teste.db');
  const abertas = [];
  const abrirBase = () => {
    const db = abrir(ficheiro);
    abertas.push(db);
    return db;
  };
  try {
    return await fn(abrirBase, ficheiro);
  } finally {
    for (const db of abertas) {
      try {
        db.close();
      } catch {} // já estava fechada pelo próprio teste
    }
    rmSync(pasta, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
  }
}
