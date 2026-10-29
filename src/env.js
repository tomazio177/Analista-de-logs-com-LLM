import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseEnv } from 'node:util';
import { pastaDeDados } from './historico.js';

// A chave da API vem de um ficheiro .env: o da pasta de dados (onde a app a guarda) ou o da pasta do projeto
// (como no início). Sem nenhum, fica a que estiver no ambiente.
// No Windows o .env costuma sair do Bloco de Notas como ".env.txt", ou do PowerShell em UTF-16 ou com BOM;
// em vez de falhar em silêncio, lê esses casos ou diz o que está mal.
const PROJETO = fileURLToPath(new URL('../', import.meta.url));

function lerEnv(pasta) {
  const env = path.join(pasta, '.env');
  if (!existsSync(env)) {
    return existsSync(path.join(pasta, '.env.txt'))
      ? { problema: `Em ${pasta}, o ficheiro chama-se ".env.txt". Muda-lhe o nome: Rename-Item .env.txt .env` }
      : { valores: {} };
  }
  const bytes = readFileSync(env);
  if (bytes[0] === 0xfe && bytes[1] === 0xff) return { problema: `O ${env} está em UTF-16 big-endian. Grava-o outra vez em UTF-8.` };
  const texto = bytes[0] === 0xff && bytes[1] === 0xfe ? bytes.subarray(2).toString('utf16le') : bytes.toString('utf8').replace(/^\uFEFF/, '');
  return { valores: parseEnv(texto) };
}

// Devolve null se a chave ficou carregada, ou o que está mal.
export function carregarEnv() {
  const problemas = [];
  for (const pasta of [pastaDeDados(), PROJETO]) {
    const { valores, problema } = lerEnv(pasta);
    if (problema) {
      problemas.push(problema);
      continue;
    }
    for (const [nome, valor] of Object.entries(valores)) process.env[nome] ??= valor.trim();
  }
  if (process.env.ANTHROPIC_API_KEY) return null;
  problemas.push(`Não encontrei a ANTHROPIC_API_KEY no .env de ${pastaDeDados()} nem no de ${PROJETO}.`);
  return problemas.join(' ');
}

// Guarda a chave no .env da pasta de dados, mantendo o resto do ficheiro.
export function escreverChave(chave) {
  const pasta = pastaDeDados();
  const env = path.join(pasta, '.env');
  mkdirSync(pasta, { recursive: true });
  const outras = existsSync(env) ? readFileSync(env, 'utf8').split(/\r?\n/).filter((l) => l.trim() && !/^\s*ANTHROPIC_API_KEY\s*=/.test(l)) : [];
  writeFileSync(env, [...outras, `ANTHROPIC_API_KEY=${chave}`, ''].join('\n'), { encoding: 'utf8', mode: 0o600 });
  process.env.ANTHROPIC_API_KEY = chave;
  return env;
}
