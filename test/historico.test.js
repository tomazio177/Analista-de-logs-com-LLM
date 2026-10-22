import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { DatabaseSync } from 'node:sqlite';
import { abrir, guardar, guardarTriagem, aparelhosConhecidos, dataDoUltimoVarrimento } from '../src/historico.js';

function comBaseTemporaria(fn) {
  const pasta = mkdtempSync(path.join(tmpdir(), 'analista-'));
  try {
    return fn(path.join(pasta, 'teste.db'));
  } finally {
    rmSync(pasta, { recursive: true, force: true });
  }
}

test('aparelhosConhecidos devolve as portas da última vez que cada aparelho foi visto', () =>
  comBaseTemporaria((ficheiro) => {
    const db = abrir(ficheiro);
    const aparelho = (mac, ip, portas) => ({ mac, ip, portas, nome: null });
    guardar(db, 'casa', [aparelho('aa', '1', [80]), aparelho('bb', '2', [22])], '2026-10-20T10:00:00Z');
    guardar(db, 'escritorio', [aparelho('aa', '9', [80, 443])], '2026-10-21T10:00:00Z');

    assert.deepEqual([...aparelhosConhecidos(db)], [['aa', [80, 443]], ['bb', [22]]]);
    assert.equal(dataDoUltimoVarrimento(db), '2026-10-21T10:00:00Z');
  }));

test('a tabela triagens aceita falhas sem resposta', () =>
  comBaseTemporaria((ficheiro) => {
    const db = abrir(ficheiro);
    const id = guardar(db, 'casa', []);
    guardarTriagem(db, id, { modelo: 'm', entrada: { sinais: [] }, resposta: null, erro: 'sem ligação' });
    assert.deepEqual({ ...db.prepare('SELECT resposta, erro FROM triagens').get() }, { resposta: null, erro: 'sem ligação' });
  }));

test('uma base criada pela primeira versão da fase 2 é migrada sem perder triagens', () =>
  comBaseTemporaria((ficheiro) => {
    const antiga = new DatabaseSync(ficheiro);
    antiga.exec(`
      CREATE TABLE varrimentos (id INTEGER PRIMARY KEY, data TEXT NOT NULL, rede TEXT NOT NULL);
      CREATE TABLE triagens (varrimento_id INTEGER PRIMARY KEY REFERENCES varrimentos(id), modelo TEXT NOT NULL, entrada TEXT NOT NULL, resposta TEXT NOT NULL);
      INSERT INTO varrimentos VALUES (1, '2026-10-22T10:00:00Z', 'casa');
      INSERT INTO triagens VALUES (1, 'm', '{}', '{"resumo":"ok"}');
    `);
    antiga.close();

    const db = abrir(ficheiro);
    const id = guardar(db, 'casa', []);
    guardarTriagem(db, id, { modelo: 'm', entrada: {}, resposta: null, erro: 'recusa' });
    assert.deepEqual(
      db.prepare('SELECT varrimento_id, resposta, erro FROM triagens ORDER BY varrimento_id').all().map((r) => ({ ...r })),
      [{ varrimento_id: 1, resposta: '{"resumo":"ok"}', erro: null }, { varrimento_id: 2, resposta: null, erro: 'recusa' }],
    );
    db.close();
    assert.doesNotThrow(() => abrir(ficheiro).close()); // abrir outra vez não volta a migrar
  }));
