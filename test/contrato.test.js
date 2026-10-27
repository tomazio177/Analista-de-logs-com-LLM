import { test } from 'node:test';
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { comBaseTemporaria } from './ajuda.js';
import { guardar, guardarNota } from '../src/historico.js';
import { fazerTriagem, perguntas, responder, respostas, apagarResposta, lerLogs, CONTRATO } from '../src/motor.js';

const correr = promisify(execFile);

// Corre um comando como a app o vai correr: com --json, numa base própria. Nunca a triagem a sério (gastaria a chave do .env).
async function comando(ficheiro, script, ...args) {
  try {
    const { stdout } = await correr(process.execPath, ['--disable-warning=ExperimentalWarning', `src/${script}`, ...args, '--json'], { env: { ...process.env, ANALISTA_DB: ficheiro } });
    return { saida: 0, documento: JSON.parse(stdout) };
  } catch (erro) {
    return { saida: erro.code, documento: JSON.parse(erro.stdout) };
  }
}

function clienteFalso(triagem) {
  return { beta: { messages: { create: async () => ({ model: 'm', stop_reason: 'end_turn', usage: { input_tokens: 10, output_tokens: 20 }, content: [{ type: 'text', text: JSON.stringify(triagem) }] }) } } };
}
const resposta = {
  nivel_geral: 'tranquilo',
  resumo: 'Nada de grave.',
  achados: [{
    titulo: 'Portátil novo', gravidade: 'baixa', alertas_relacionados: [], nos_dados: ['192.168.1.221, LAPTOP-AB12CD34.lan'],
    conhecimento_geral: [], conclusao: 'Provavelmente de casa.', acao_sugerida: 'Confirmar.', confianca: 'media',
  }],
  perguntas_ao_dono: [{ pergunta: 'De quem é o LAPTOP-AB12CD34?', sobre: 'd2' }, { pergunta: 'Mais alguém usa este PC?', sobre: null }],
};
const casa = (db) => guardar(db, { cidr: '192.168.1.0/24', router: 'r' }, [
  { mac: '00:00:5e:00:53:4d', ip: '192.168.1.77', portas: [445], nome: 'PC-Sala.lan', proprio: true },
  { mac: '00:00:5e:00:53:dd', ip: '192.168.1.221', portas: [], nome: 'LAPTOP-AB12CD34.lan', proprio: false },
]);

test('com --json sai um só documento, com a versão do contrato e um código de erro estável', () =>
  comBaseTemporaria(async (abrirBase, ficheiro) => {
    abrirBase().close();
    const semTriagem = await comando(ficheiro, 'notas.js', '--perguntas');
    assert.deepEqual(semTriagem, { saida: 1, documento: { contrato: CONTRATO, ok: false, erro: { codigo: 'sem-triagem', mensagem: semTriagem.documento.erro.mensagem } } });
    assert.equal((await comando(ficheiro, 'triar.js', '--ver-dados')).documento.erro.codigo, 'sem-varrimentos');
  }));

test('a triagem devolve achados, avisos e perguntas numeradas, e fica guardada', () =>
  comBaseTemporaria(async (abrirBase) => {
    const db = abrirBase();
    casa(db);
    const r = await fazerTriagem(db, { cliente: clienteFalso(resposta) });
    assert.equal(r.ok, true);
    assert.equal(r.triagem_id, 1);
    assert.deepEqual(r.uso, { entrada: 10, saida: 20 });
    assert.deepEqual(r.achados[0].factos_sem_apoio, []);
    assert.deepEqual(r.perguntas.map((p) => [p.n, p.sobre, p.respondida]), [[1, { tipo: 'aparelho', alvo: '00:00:5e:00:53:dd' }, false], [2, null, false]]);
  }));

test('uma recusa sai com código próprio e fica guardada como falha', () =>
  comBaseTemporaria(async (abrirBase) => {
    const db = abrirBase();
    casa(db);
    const recusa = { beta: { messages: { create: async () => ({ stop_reason: 'refusal', content: [] }) } } };
    assert.equal((await fazerTriagem(db, { cliente: recusa })).erro.codigo, 'recusa');
    assert.match(db.prepare('SELECT erro FROM triagens').get().erro, /recusou/);
    assert.equal(perguntas(db).erro.codigo, 'sem-triagem'); // uma falha não conta como triagem com perguntas
  }));

test('responder sem conversa: a pergunta fica respondida e a resposta presa ao aparelho', () =>
  comBaseTemporaria(async (abrirBase, ficheiro) => {
    const db = abrirBase();
    casa(db);
    await fazerTriagem(db, { cliente: clienteFalso(resposta) });

    assert.equal(responder(db, { n: 3, texto: 'x' }).erro.codigo, 'pergunta-inexistente');
    assert.equal(responder(db, { n: 1, texto: '   ' }).erro.codigo, 'resposta-vazia');
    // Pelo comando, como a app vai fazer.
    const respondido = await comando(ficheiro, 'notas.js', '--pergunta', '1', '--texto', 'É de alguém de casa.');
    assert.deepEqual(respondido.documento, { contrato: CONTRATO, ok: true, resposta_id: 1 });

    assert.deepEqual(perguntas(db).perguntas.map((p) => p.respondida), [true, false]);
    const listadas = (await comando(ficheiro, 'notas.js', '--listar')).documento.respostas;
    assert.deepEqual(listadas.map((n) => [n.sobre, n.texto]), [[{ tipo: 'aparelho', alvo: '00:00:5e:00:53:dd' }, 'É de alguém de casa.']]);
    assert.equal(apagarResposta(db, 1).ok, true);
    assert.equal(apagarResposta(db, 1).erro.codigo, 'resposta-inexistente');
    assert.deepEqual(respostas(db).respostas, []);
  }));

test('as respostas de antes da ligação à triagem continuam a aparecer, sem contar como respondidas', () =>
  comBaseTemporaria(async (abrirBase) => {
    const db = abrirBase();
    casa(db);
    await fazerTriagem(db, { cliente: clienteFalso(resposta) });
    guardarNota(db, { alvoTipo: 'geral', pergunta: 'Mais alguém usa este PC?', texto: 'Não.' });
    assert.deepEqual(perguntas(db).perguntas.map((p) => p.respondida), [false, false]);
    assert.equal(respostas(db).respostas.length, 1);
  }));

test('fora do Windows, ler os logins dá um erro com código, não uma exceção', { skip: process.platform === 'win32' }, () =>
  comBaseTemporaria(async (abrirBase, ficheiro) => {
    assert.equal((await lerLogs(abrirBase())).erro.codigo, 'nao-windows');
    assert.equal((await comando(ficheiro, 'logs.js')).documento.erro.codigo, 'nao-windows');
  }));
