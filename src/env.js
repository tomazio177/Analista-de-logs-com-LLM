import { existsSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { parseEnv } from 'node:util';

// A chave da API vem do ficheiro .env (fora do git). Sem .env, fica a que estiver no ambiente.
// No Windows o .env costuma sair do Bloco de Notas como ".env.txt", ou do PowerShell em UTF-16 ou com BOM;
// em vez de falhar em silêncio, lê esses casos ou diz o que está mal.
const PASTA = new URL('../', import.meta.url);

// Devolve null se a chave ficou carregada, ou o que está mal com o .env.
export function carregarEnv() {
  const env = new URL('.env', PASTA);
  if (!existsSync(env)) {
    return existsSync(new URL('.env.txt', PASTA))
      ? 'O ficheiro chama-se ".env.txt". Muda-lhe o nome: Rename-Item .env.txt .env'
      : `Não há ficheiro .env em ${fileURLToPath(PASTA)}`;
  }
  const bytes = readFileSync(env);
  let texto;
  if (bytes[0] === 0xff && bytes[1] === 0xfe) texto = bytes.subarray(2).toString('utf16le');
  else if (bytes[0] === 0xfe && bytes[1] === 0xff) return 'O .env está em UTF-16 big-endian. Grava-o outra vez em UTF-8.';
  else texto = bytes.toString('utf8').replace(/^\uFEFF/, '');

  const valores = parseEnv(texto);
  for (const [nome, valor] of Object.entries(valores)) process.env[nome] ??= valor.trim();
  if (!('ANTHROPIC_API_KEY' in valores)) {
    const nomes = Object.keys(valores).join(', ') || 'nenhuma';
    return `O .env existe mas não tem a variável ANTHROPIC_API_KEY (variáveis encontradas: ${nomes}).`;
  }
  return null;
}
