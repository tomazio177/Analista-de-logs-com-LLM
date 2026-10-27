import { test } from 'node:test';
import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { comBaseTemporaria } from './ajuda.js';
import {
  guardar, ultimoVarrimento, ausenciasPorMac, contarVarrimentos, inventarioAtual,
  aprovarRede, redeAprovada, guardarLeitura, coberturaDosLogs, guardarTriagem,
} from '../src/historico.js';

const aparelho = (mac, ip, extra = {}) => ({ mac, ip, portas: [], nome: null, ...extra });

test('uma base anterior a estas versões ganha as colunas novas sem perder varrimentos', () =>
  comBaseTemporaria((abrirBase, ficheiro) => {
    const antiga = new DatabaseSync(ficheiro);
    antiga.exec(`
      CREATE TABLE varrimentos (id INTEGER PRIMARY KEY, data TEXT NOT NULL, rede TEXT NOT NULL);
      CREATE TABLE observacoes (varrimento_id INTEGER NOT NULL, mac TEXT NOT NULL, ip TEXT NOT NULL, portas TEXT NOT NULL, PRIMARY KEY (varrimento_id, mac));
      INSERT INTO varrimentos VALUES (1, '2026-10-23T17:32:23Z', '192.168.1.0/24');
      INSERT INTO observacoes VALUES (1, 'aa', '192.168.1.77', '[445]');
    `);
    antiga.close();

    const db = abrirBase();
    const colunas = (t) => db.prepare(`PRAGMA table_info(${t})`).all().map((c) => c.name);
    assert.ok(colunas('observacoes').includes('proprio'));
    assert.ok(colunas('varrimentos').includes('router_mac'));
    assert.equal(ultimoVarrimento(db, { cidr: '192.168.1.0/24', router: 'qualquer' }).data, '2026-10-23T17:32:23Z');
    db.close();
    assert.doesNotThrow(() => abrirBase().close()); // abrir outra vez não volta a migrar
  }));

test('duas casas com o mesmo intervalo de endereços não misturam o histórico', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    const casa = { cidr: '192.168.1.0/24', router: '00:00:5e:00:53:01' };
    const amigos = { cidr: '192.168.1.0/24', router: '00:00:5e:00:53:02' };
    guardar(db, casa, [aparelho('tv', '192.168.1.20')], '2026-10-24T10:00:00Z');
    guardar(db, amigos, [aparelho('consola', '192.168.1.20')], '2026-10-25T10:00:00Z');
    guardar(db, amigos, [], '2026-10-26T10:00:00Z');

    assert.deepEqual([...ultimoVarrimento(db, casa).aparelhos.keys()], ['tv']);
    assert.equal(contarVarrimentos(db, casa).n, 1);
    assert.equal(contarVarrimentos(db, amigos).n, 2);
    // A TV não falhou nenhum varrimento da casa: os da outra rede não contam.
    assert.equal(ausenciasPorMac(db, casa).get('tv').faltas, 0);
    assert.equal(ausenciasPorMac(db, amigos).get('consola').faltas, 1);
  }));

test('só as redes aprovadas ficam aprovadas, e o PC fica marcado em cada varrimento', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    assert.equal(redeAprovada(db, '00:00:5e:00:53:01'), undefined);
    aprovarRede(db, '00:00:5e:00:53:01', '192.168.1.0/24');
    assert.equal(redeAprovada(db, '00:00:5e:00:53:01').rede, '192.168.1.0/24');
    assert.equal(redeAprovada(db, '00:00:5e:00:53:02'), undefined);

    guardar(db, { cidr: '192.168.1.0/24', router: '00:00:5e:00:53:01' }, [aparelho('pc', '192.168.1.77', { proprio: true }), aparelho('tv', '192.168.1.20')]);
    assert.deepEqual(inventarioAtual(db).aparelhos.map((a) => [a.mac, a.proprio]), [['pc', 1], ['tv', 0]]);
  }));

test('os intervalos que nenhuma leitura dos logs cobre aparecem como buracos', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    const desde = '2026-10-20T00:00:00Z';
    assert.deepEqual(coberturaDosLogs(db, desde), { ultimaLeitura: null, buracos: [] });

    // A primeira leitura já só encontrou eventos a partir de 22/10: o início do período fica por cobrir.
    guardarLeitura(db, '2026-10-27T10:00:00Z', '2026-10-22T14:55:28Z');
    // A seguinte ainda apanha a anterior: não há buraco novo.
    guardarLeitura(db, '2026-10-28T10:00:00Z', '2026-10-24T00:00:00Z');
    // Ficou uma semana sem ler e o registo rodou: de 28/10 a 30/10 perdeu-se.
    guardarLeitura(db, '2026-11-05T10:00:00Z', '2026-10-30T00:00:00Z');

    assert.deepEqual(coberturaDosLogs(db, desde), {
      ultimaLeitura: '2026-11-05T10:00:00Z',
      buracos: [{ de: desde, ate: '2026-10-22T14:55:28Z' }, { de: '2026-10-28T10:00:00Z', ate: '2026-10-30T00:00:00Z' }],
    });
    // Buracos anteriores ao período pedido ficam de fora, e os que o atravessam são cortados no início.
    assert.deepEqual(coberturaDosLogs(db, '2026-10-29T00:00:00Z').buracos, [{ de: '2026-10-29T00:00:00Z', ate: '2026-10-30T00:00:00Z' }]);
  }));

test('a tabela triagens guarda o que foi enviado e também as falhas sem resposta', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    guardar(db, { cidr: 'r', router: 'x' }, []);
    guardarTriagem(db, { varrimentoId: 1, modelo: 'm', entrada: { aparelhos: [] }, resposta: '{"resumo":"ok"}', erro: null }, '2026-10-27T10:00:00Z');
    guardarTriagem(db, { varrimentoId: 1, modelo: 'm', entrada: { aparelhos: [] }, resposta: null, erro: 'recusa' }, '2026-10-27T11:00:00Z');

    assert.deepEqual(
      db.prepare('SELECT varrimento_id, entrada, resposta, erro FROM triagens ORDER BY id').all().map((r) => ({ ...r })),
      [
        { varrimento_id: 1, entrada: '{"aparelhos":[]}', resposta: '{"resumo":"ok"}', erro: null },
        { varrimento_id: 1, entrada: '{"aparelhos":[]}', resposta: null, erro: 'recusa' },
      ],
    );
  }));
