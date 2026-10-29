import { test } from 'node:test';
import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync, existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import Anthropic from '@anthropic-ai/sdk';
import { pastaDeDados, ficheiroPorOmissao } from '../src/historico.js';
import { guardarChave, configuracao } from '../src/motor.js';

// Cada teste numa pasta de dados própria, sem tocar na do utilizador nem deixar uma chave no ambiente.
async function comPastaDeDados(fn) {
  const pasta = mkdtempSync(path.join(tmpdir(), 'analista-dados-'));
  const antes = { dados: process.env.ANALISTA_DADOS, chave: process.env.ANTHROPIC_API_KEY };
  process.env.ANALISTA_DADOS = pasta;
  delete process.env.ANTHROPIC_API_KEY;
  try {
    return await fn(pasta);
  } finally {
    for (const [nome, valor] of [['ANALISTA_DADOS', antes.dados], ['ANTHROPIC_API_KEY', antes.chave]]) {
      if (valor === undefined) delete process.env[nome];
      else process.env[nome] = valor;
    }
    rmSync(pasta, { recursive: true, force: true });
  }
}

const aceita = { models: { list: async () => ({ data: [] }) } };
const falhaCom = (prototipo) => ({ models: { list: async () => { throw Object.create(prototipo); } } });
const CHAVE = 'sk-ant-api03-exemplo_DE-teste';

test('os dados vão para a pasta indicada, e a base antiga é copiada para lá uma única vez', () =>
  comPastaDeDados((pasta) => {
    assert.equal(pastaDeDados(), pasta);
    const antiga = path.join(pasta, 'antiga.db');
    writeFileSync(antiga, 'base antiga');
    const nova = ficheiroPorOmissao(antiga);
    assert.equal(nova, path.join(pasta, 'analista.db'));
    assert.equal(readFileSync(nova, 'utf8'), 'base antiga');
    // Se a nova já existe, a antiga não a substitui.
    writeFileSync(nova, 'base nova');
    ficheiroPorOmissao(antiga);
    assert.equal(readFileSync(nova, 'utf8'), 'base nova');
    assert.equal(existsSync(antiga), true); // a antiga fica como cópia de segurança
  }));

test('uma chave com mau aspeto ou recusada pela Anthropic não é guardada', () =>
  comPastaDeDados(async (pasta) => {
    assert.equal((await guardarChave('   ', { cliente: aceita })).erro.codigo, 'chave-vazia');
    assert.equal((await guardarChave('a minha chave', { cliente: aceita })).erro.codigo, 'chave-invalida');
    assert.equal((await guardarChave(CHAVE, { cliente: falhaCom(Anthropic.AuthenticationError.prototype) })).erro.codigo, 'chave-recusada');
    assert.equal(existsSync(path.join(pasta, '.env')), false);
  }));

test('uma chave aceite fica no .env da pasta de dados, sem apagar o resto do ficheiro', () =>
  comPastaDeDados(async (pasta) => {
    writeFileSync(path.join(pasta, '.env'), 'OUTRA=1\nANTHROPIC_API_KEY=sk-ant-velha\n');
    const r = await guardarChave(`  ${CHAVE}\n`, { cliente: aceita });
    assert.deepEqual(r, { ok: true, pasta_de_dados: pasta });
    assert.equal(readFileSync(path.join(pasta, '.env'), 'utf8'), `OUTRA=1\nANTHROPIC_API_KEY=${CHAVE}\n`);
    assert.equal(configuracao().chave_definida, true);
  }));

test('sem internet a chave fica guardada na mesma, com aviso', () =>
  comPastaDeDados(async (pasta) => {
    const r = await guardarChave(CHAVE, { cliente: falhaCom(Anthropic.APIConnectionError.prototype) });
    assert.equal(r.ok, true);
    assert.match(r.aviso, /sem ligação|não há ligação/);
    assert.match(readFileSync(path.join(pasta, '.env'), 'utf8'), /ANTHROPIC_API_KEY=sk-ant-api03/);
  }));

test('pelo comando, a chave entra pelo stdin e o resultado sai no contrato', () =>
  comPastaDeDados(async (pasta) => {
    const correr = (args, entrada) =>
      new Promise((resolve) => {
        const filho = execFile(process.execPath, ['--disable-warning=ExperimentalWarning', 'src/config.js', ...args, '--json'], { env: { ...process.env, ANALISTA_DADOS: pasta } }, (erro, stdout) => resolve(JSON.parse(stdout)));
        filho.stdin.end(entrada ?? '');
      });
    assert.equal((await correr([])).pasta_de_dados, pasta);
    // Uma chave com mau aspeto não chega a ir à Anthropic: o teste não precisa de internet.
    assert.equal((await correr(['--guardar-chave'], 'não é uma chave\n')).erro.codigo, 'chave-invalida');
  }));
