import { DatabaseSync } from 'node:sqlite';
import { mkdirSync } from 'node:fs';
import path from 'node:path';

// ANALISTA_DB troca a base de dados (os testes usam uma temporária; a app pode querer outra pasta).
const FICHEIRO = process.env.ANALISTA_DB ?? path.join(import.meta.dirname, '..', 'data', 'analista.db');

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
    CREATE TABLE IF NOT EXISTS redes_aprovadas (
      router_mac   TEXT PRIMARY KEY,
      rede         TEXT NOT NULL,
      aprovada_em  TEXT NOT NULL
    );
    CREATE TABLE IF NOT EXISTS triagens (
      id             INTEGER PRIMARY KEY,
      data           TEXT NOT NULL,
      varrimento_id  INTEGER REFERENCES varrimentos(id),
      modelo         TEXT,
      entrada        TEXT NOT NULL,
      resposta       TEXT,
      erro           TEXT
    );
    CREATE TABLE IF NOT EXISTS notas (
      id         INTEGER PRIMARY KEY,
      data       TEXT NOT NULL,
      alvo_tipo  TEXT NOT NULL,
      alvo       TEXT,
      pergunta   TEXT,
      texto      TEXT NOT NULL,
      triagem_id INTEGER REFERENCES triagens(id)
    );
    CREATE TABLE IF NOT EXISTS leituras_logs (
      data         TEXT NOT NULL,
      mais_antigo  TEXT
    );
    CREATE TABLE IF NOT EXISTS logins_falhados (
      record_id   INTEGER NOT NULL,
      quando      TEXT NOT NULL,
      utilizador  TEXT,
      tipo        INTEGER,
      codigo      TEXT,
      ip          TEXT,
      estacao     TEXT,
      processo    TEXT,
      PRIMARY KEY (record_id, quando)
    );
  `);
  // Bases criadas antes de se guardar qual é este PC em cada varrimento.
  if (!db.prepare('PRAGMA table_info(observacoes)').all().some((c) => c.name === 'proprio')) {
    db.exec('ALTER TABLE observacoes ADD COLUMN proprio INTEGER NOT NULL DEFAULT 0');
  }
  // E antes de as triagens guardarem a que aparelho ou alerta se refere cada id (para as respostas do dono).
  if (!db.prepare('PRAGMA table_info(triagens)').all().some((c) => c.name === 'ancoras')) {
    db.exec('ALTER TABLE triagens ADD COLUMN ancoras TEXT');
  }
  // E antes de cada nota guardar a que triagem respondeu (para saber que perguntas já têm resposta).
  if (!db.prepare('PRAGMA table_info(notas)').all().some((c) => c.name === 'triagem_id')) {
    db.exec('ALTER TABLE notas ADD COLUMN triagem_id INTEGER REFERENCES triagens(id)');
  }
  // E antes de a rede ser identificada pelo MAC do router.
  if (!db.prepare('PRAGMA table_info(varrimentos)').all().some((c) => c.name === 'router_mac')) {
    db.exec('ALTER TABLE varrimentos ADD COLUMN router_mac TEXT');
  }
  return db;
}

// MAC → último nome conhecido (ou null) de todos os aparelhos já vistos.
export function macsConhecidos(db) {
  return new Map(db.prepare('SELECT mac, nome FROM dispositivos').all().map((r) => [r.mac, r.nome]));
}

// Uma rede é o intervalo de endereços mais o MAC do router: muitas casas usam 192.168.1.0/24,
// e só o intervalo misturaria o histórico de casas diferentes. Os varrimentos de antes desta mudança
// não têm router guardado e contam como da rede com o mesmo intervalo.
const DA_REDE = '(v.rede = :cidr AND (v.router_mac = :router OR v.router_mac IS NULL))';

export const redeAprovada = (db, routerMac) =>
  db.prepare('SELECT rede, aprovada_em FROM redes_aprovadas WHERE router_mac = ?').get(routerMac);

export function aprovarRede(db, routerMac, cidr) {
  db.prepare('INSERT OR REPLACE INTO redes_aprovadas (router_mac, rede, aprovada_em) VALUES (?, ?, ?)')
    .run(routerMac, cidr, new Date().toISOString());
}

export const contarVarrimentos = (db, rede) =>
  db.prepare(`SELECT COUNT(*) AS n, MIN(v.data) AS primeiro FROM varrimentos v WHERE ${DA_REDE}`).get(rede);

// Por cada aparelho já visto nesta rede: quantos varrimentos desta rede aconteceram desde a última vez que respondeu.
// Chamar ANTES de `guardar`, para o varrimento em curso não contar.
export function ausenciasPorMac(db, rede) {
  const linhas = db.prepare(`
    SELECT d.mac, d.ultimo_ip AS ip,
           (SELECT COUNT(*) FROM varrimentos v WHERE ${DA_REDE} AND v.data > d.ultimo_visto) AS faltas
    FROM dispositivos d
    WHERE EXISTS (
      SELECT 1 FROM observacoes o JOIN varrimentos v ON v.id = o.varrimento_id
      WHERE o.mac = d.mac AND ${DA_REDE}
    )
  `).all(rede);
  return new Map(linhas.map((l) => [l.mac, { faltas: l.faltas, ip: l.ip }]));
}

export function ultimoVarrimento(db, rede) {
  const v = db.prepare(`SELECT id, data FROM varrimentos v WHERE ${DA_REDE} ORDER BY id DESC LIMIT 1`).get(rede);
  if (!v) return null;

  const aparelhos = new Map();
  for (const o of db.prepare('SELECT mac, ip, portas FROM observacoes WHERE varrimento_id = ?').all(v.id)) {
    aparelhos.set(o.mac, { ip: o.ip, portas: JSON.parse(o.portas) });
  }
  return { data: v.data, aparelhos };
}

export function guardar(db, rede, aparelhos, data = new Date().toISOString()) {
  const novoVarrimento = db.prepare('INSERT INTO varrimentos (data, rede, router_mac) VALUES (?, ?, ?)');
  const observacao = db.prepare('INSERT INTO observacoes (varrimento_id, mac, ip, portas, proprio) VALUES (?, ?, ?, ?, ?)');
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
    const { lastInsertRowid: id } = novoVarrimento.run(data, rede.cidr, rede.router);
    for (const a of aparelhos) {
      observacao.run(id, a.mac, a.ip, JSON.stringify(a.portas), a.proprio ? 1 : 0);
      dispositivo.run(a.mac, data, data, a.ip, a.nome);
    }
    db.exec('COMMIT');
  } catch (erro) {
    db.exec('ROLLBACK');
    throw erro;
  }
}

// Guarda os logins falhados e devolve os ids dos que ainda não estavam guardados.
// A chave inclui a data porque o Windows pode reutilizar números de registo se o log for limpo.
export function guardarLogins(db, logins) {
  const inserir = db.prepare(`
    INSERT OR IGNORE INTO logins_falhados (record_id, quando, utilizador, tipo, codigo, ip, estacao, processo)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  `);
  const novos = new Set();
  db.exec('BEGIN');
  try {
    for (const l of logins) {
      const { changes } = inserir.run(l.id, l.quando, l.utilizador, l.tipo, l.codigo, l.ip, l.estacao, l.processo);
      if (changes) novos.add(l.id);
    }
    db.exec('COMMIT');
  } catch (erro) {
    db.exec('ROLLBACK');
    throw erro;
  }
  return novos;
}

// IP → aparelho que o teve da última vez que foi visto num varrimento. Serve para pôr nome às origens dos logins.
// Os IPs mudam com o tempo, por isso é uma pista: o mais recente ganha.
export function aparelhosPorIp(db) {
  const linhas = db.prepare('SELECT mac, nome, ultimo_ip FROM dispositivos ORDER BY ultimo_visto').all();
  return new Map(linhas.map((l) => [l.ultimo_ip, { mac: l.mac, nome: l.nome }]));
}

// O último varrimento com os dados de cada aparelho, para a triagem.
export function inventarioAtual(db) {
  const v = db.prepare('SELECT id, data, rede, router_mac FROM varrimentos ORDER BY id DESC LIMIT 1').get();
  if (!v) return null;
  const aparelhos = db.prepare(`
    SELECT o.ip, o.mac, o.portas, o.proprio, d.nome, d.primeiro_visto
    FROM observacoes o JOIN dispositivos d ON d.mac = o.mac
    WHERE o.varrimento_id = ?
  `).all(v.id).map((a) => ({ ...a, portas: JSON.parse(a.portas) }));
  return { ...v, aparelhos };
}

export function loginsDesde(db, desde) {
  return db.prepare(`
    SELECT record_id AS id, quando, utilizador, tipo, codigo, ip, estacao, processo
    FROM logins_falhados WHERE quando >= ? ORDER BY quando
  `).all(desde);
}

// Cada leitura dos logs guarda até onde o registo do Windows chegava nesse momento.
export function guardarLeitura(db, data, maisAntigo) {
  db.prepare('INSERT INTO leituras_logs (data, mais_antigo) VALUES (?, ?)').run(data, maisAntigo);
}

// Intervalos desde `desde` em que o Windows apagou eventos antes de uma leitura os apanhar:
// uma leitura cujo evento mais antigo é posterior à leitura anterior (ou ao início do período, na primeira) deixa um buraco.
// Leituras feitas antes de existir esta tabela não contam, por isso o início tende a aparecer como buraco: é o lado seguro.
export function coberturaDosLogs(db, desde) {
  const leituras = db.prepare('SELECT data, mais_antigo FROM leituras_logs ORDER BY data').all();
  const buracos = [];
  let coberto = null;
  for (const l of leituras) {
    const inicio = coberto ?? desde;
    if (l.mais_antigo && l.mais_antigo > inicio && l.mais_antigo > desde) {
      buracos.push({ de: inicio > desde ? inicio : desde, ate: l.mais_antigo });
    }
    coberto = l.data;
  }
  return { ultimaLeitura: coberto, buracos };
}

// Cada triagem fica guardada com o que foi enviado e o texto devolvido, tal e qual, mesmo quando falha:
// dá para ver mais tarde o que o modelo disse e com que dados.
export function guardarTriagem(db, { varrimentoId, modelo, entrada, resposta, erro, ancoras = null }, data = new Date().toISOString()) {
  const { lastInsertRowid } = db.prepare('INSERT INTO triagens (data, varrimento_id, modelo, entrada, resposta, erro, ancoras) VALUES (?, ?, ?, ?, ?, ?, ?)')
    .run(data, varrimentoId, modelo, JSON.stringify(entrada), resposta, erro, ancoras && JSON.stringify(ancoras));
  return Number(lastInsertRowid);
}

export const ultimaTriagemValida = (db) =>
  db.prepare('SELECT id, data, resposta, ancoras FROM triagens WHERE erro IS NULL AND resposta IS NOT NULL ORDER BY id DESC LIMIT 1').get();

// As respostas do dono às perguntas da triagem. Cada nota fica presa àquilo de que fala:
// um aparelho (pelo MAC), um alerta concreto (tipo, origem e hora de início) ou nada em particular ("geral").
// Assim "a rajada de 22/10 fui eu" não serve para desculpar uma rajada nova.
export function guardarNota(db, { alvoTipo, alvo = null, pergunta = null, texto, triagemId = null }, data = new Date().toISOString()) {
  const { lastInsertRowid } = db.prepare('INSERT INTO notas (data, alvo_tipo, alvo, pergunta, texto, triagem_id) VALUES (?, ?, ?, ?, ?, ?)')
    .run(data, alvoTipo, alvo, pergunta, texto, triagemId);
  return Number(lastInsertRowid);
}

export const listarNotas = (db) => db.prepare('SELECT id, data, alvo_tipo, alvo, pergunta, texto, triagem_id FROM notas ORDER BY id').all();

export const apagarNota = (db, id) => db.prepare('DELETE FROM notas WHERE id = ?').run(id).changes > 0;

// "tipo:alvo" → notas, para juntar a cada aparelho e alerta nos dados da triagem.
export function notasPorAlvo(db) {
  const mapa = new Map();
  for (const n of listarNotas(db)) {
    const chave = `${n.alvo_tipo}:${n.alvo ?? ''}`;
    mapa.set(chave, [...(mapa.get(chave) ?? []), { quando: n.data, texto: n.texto }]);
  }
  return mapa;
}
