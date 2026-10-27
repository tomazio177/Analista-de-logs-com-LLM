import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';

// Cada teste trabalha numa base de dados própria, numa pasta temporária que desaparece no fim.
export async function comBaseTemporaria(fn) {
  const pasta = mkdtempSync(path.join(tmpdir(), 'analista-'));
  try {
    return await fn(path.join(pasta, 'teste.db'));
  } finally {
    rmSync(pasta, { recursive: true, force: true });
  }
}
