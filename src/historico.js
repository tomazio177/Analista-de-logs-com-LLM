import { DatabaseSync } from 'node:sqlite';
import { mkdirSync } from 'node:fs';
import path from 'node:path';

const FICHEIRO = path.join(import.meta.dirname, '..', 'data', 'sentinela.db');

export function abrir(ficheiro = FICHEIRO) {
  mkdirSync(path.dirname(ficheiro), { recursive: true });
  const db = new DatabaseSync(ficheiro);
  db.exec(`
    CREATE TABLE IF NOT EXISTS varrimentos (
      id    INTEGER PRIMARY KEY,
      data  TEXT NOT NULL,
      rede  TEXT NOT NULL
    );
    CREATE TABLE IF NOT EXISTS dispositivos (
      mac             TEXT PRIMARY KEY,
      primeiro_visto  TEXT NOT NULL,
      ultimo_visto    TEXT NOT NULL,
      ultimo_ip       TEXT,
      nome            TEXT
    );
    CREATE TABLE IF NOT EXISTS observacoes (
      varrimento_id  INTEGER NOT NULL REFERENCES varrimentos(id),
      mac            TEXT NOT NULL,
      ip             TEXT NOT NULL,
      portas         TEXT NOT NULL,
      PRIMARY KEY (varrimento_id, mac)
    );
    CREATE TABLE IF NOT EXISTS triagens (
      varrimento_id  INTEGER PRIMARY KEY REFERENCES varrimentos(id),
      modelo         TEXT NOT NULL,
      entrada        TEXT NOT NULL,
      resposta       TEXT NOT NULL
    );
  `);
  return db;
}

export function macsConhecidos(db) {
  return new Set(db.prepare('SELECT mac FROM dispositivos').all().map((r) => r.mac));
}

export function ultimoVarrimento(db, rede) {
  const v = db.prepare('SELECT id, data FROM varrimentos WHERE rede = ? ORDER BY id DESC LIMIT 1').get(rede);
  if (!v) return null;

  const aparelhos = new Map();
  for (const o of db.prepare('SELECT mac, ip, portas FROM observacoes WHERE varrimento_id = ?').all(v.id)) {
    aparelhos.set(o.mac, { ip: o.ip, portas: JSON.parse(o.portas) });
  }
  return { data: v.data, aparelhos };
}

export function guardar(db, rede, aparelhos, data = new Date().toISOString()) {
  const novoVarrimento = db.prepare('INSERT INTO varrimentos (data, rede) VALUES (?, ?)');
  const observacao = db.prepare('INSERT INTO observacoes (varrimento_id, mac, ip, portas) VALUES (?, ?, ?, ?)');
  const dispositivo = db.prepare(`
    INSERT INTO dispositivos (mac, primeiro_visto, ultimo_visto, ultimo_ip, nome)
    VALUES (?, ?, ?, ?, ?)
    ON CONFLICT(mac) DO UPDATE SET
      ultimo_visto = excluded.ultimo_visto,
      ultimo_ip    = excluded.ultimo_ip,
      nome         = COALESCE(excluded.nome, dispositivos.nome)
  `);

  db.exec('BEGIN');
  try {
    const { lastInsertRowid: id } = novoVarrimento.run(data, rede);
    for (const a of aparelhos) {
      observacao.run(id, a.mac, a.ip, JSON.stringify(a.portas));
      dispositivo.run(a.mac, data, data, a.ip, a.nome);
    }
    db.exec('COMMIT');
    return id;
  } catch (erro) {
    db.exec('ROLLBACK');
    throw erro;
  }
}

// Guarda o que foi ao modelo e o que ele respondeu, tal e qual.
// Sem isto não há como saber, mais tarde, se o modelo acertou — nem como comparar modelos ou instruções.
export function guardarTriagem(db, varrimentoId, modelo, entrada, resposta) {
  db.prepare('INSERT INTO triagens (varrimento_id, modelo, entrada, resposta) VALUES (?, ?, ?, ?)').run(
    varrimentoId,
    modelo,
    JSON.stringify(entrada),
    JSON.stringify(resposta),
  );
}
