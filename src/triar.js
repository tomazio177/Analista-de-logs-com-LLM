import Anthropic from '@anthropic-ai/sdk';
import { existsSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { parseEnv } from 'node:util';
import { abrir, guardarTriagem } from './historico.js';
import { recolher, triar, aplicarRegras, ErroTriagem, MODELO } from './triagem.js';
import { semApoioNosDados } from './verificar.js';

// A chave da API vem do ficheiro .env (fora do git). Sem .env, fica a que estiver no ambiente.
// No Windows o .env costuma sair do Bloco de Notas como ".env.txt", ou do PowerShell em UTF-16 ou com BOM;
// em vez de falhar em silêncio, lê esses casos ou diz o que está mal.
const PASTA = new URL('../', import.meta.url);
const problemaEnv = carregarEnv();

function carregarEnv() {
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

const db = abrir();
const { varrimentoId, dados } = recolher(db) ?? {};
if (!dados) {
  console.error('\nAinda não há varrimentos guardados. Corre primeiro: npm run scan\n');
  process.exit(1);
}

// Mostra exatamente o que seria enviado, sem gastar nada.
if (process.argv.includes('--ver-dados')) {
  console.log(JSON.stringify(dados, null, 2));
  process.exit(0);
}

if (!process.env.ANTHROPIC_API_KEY) {
  if (problemaEnv) console.error(`\n${problemaEnv}`);
  console.error('\nFalta a chave da API. Cria um ficheiro .env na pasta do projeto com uma linha:');
  console.error('  ANTHROPIC_API_KEY=sk-ant-...');
  console.error('O .env já está no .gitignore, por isso não vai parar ao GitHub.\n');
  process.exit(1);
}

console.log(`\nA enviar ${dados.aparelhos.length} aparelhos e ${dados.logins_falhados.total} logins falhados para triagem…`);

const client = new Anthropic();
let resultado;
try {
  resultado = await triar(dados, client);
} catch (erro) {
  let mensagem;
  if (erro instanceof ErroTriagem) mensagem = erro.message;
  else if (erro instanceof Anthropic.AuthenticationError) mensagem = 'A chave da API foi recusada. Confirma o valor no .env.';
  else if (erro instanceof Anthropic.RateLimitError) mensagem = 'Limite de pedidos atingido. Tenta daqui a um minuto.';
  else if (erro instanceof Anthropic.APIConnectionError) mensagem = 'Não consegui ligar à API. Confirma a ligação à internet.';
  else if (erro instanceof Anthropic.APIError) mensagem = `Erro da API (${erro.status}): ${erro.message}`;
  else throw erro;
  // As falhas também ficam guardadas: uma recusa ou uma resposta fora do esquema é informação sobre o modelo.
  guardarTriagem(db, { varrimentoId, modelo: MODELO, entrada: dados, resposta: erro.bruto ?? null, erro: mensagem });
  console.error(`\n${mensagem}\n`);
  process.exit(1);
}
guardarTriagem(db, { varrimentoId, modelo: resultado.modelo, entrada: dados, resposta: resultado.bruto, erro: null });
mostrar({ ...resultado, ...aplicarRegras(dados, resultado.triagem) });

function mostrar({ triagem: t, avisos, modelo, uso }) {
  const NIVEL = { tranquilo: '✓ TRANQUILO', atencao: '! ATENÇÃO', urgente: '!! URGENTE' };
  const ORDEM = { alta: 0, media: 1, baixa: 2, info: 3 };
  const GRAVIDADE = { alta: 'ALTA ', media: 'MÉDIA', baixa: 'BAIXA', info: 'INFO ' };

  console.log(`\n${NIVEL[t.nivel_geral]}\n\n${t.resumo}\n`);
  for (const a of t.achados.toSorted((x, y) => ORDEM[x.gravidade] - ORDEM[y.gravidade])) {
    console.log(`[${GRAVIDADE[a.gravidade]}] ${a.titulo}   (confiança ${a.confianca})`);
    if (a.gravidade_do_modelo) console.log(`  (o modelo deu ${a.gravidade_do_modelo}; num alerta vindo da rede, as regras não deixam baixar de ${a.gravidade})`);
    console.log('  Nos dados:');
    for (const f of a.nos_dados) console.log(`    · ${f}`);
    if (a.conhecimento_geral.length) {
      console.log('  Conhecimento geral (não vem dos teus dados):');
      for (const c of a.conhecimento_geral) console.log(`    · ${c}`);
    }
    console.log(`  Conclusão:   ${a.conclusao}`);
    console.log(`  O que fazer: ${a.acao_sugerida}`);
    const falta = semApoioNosDados(a.nos_dados, dados);
    if (falta.length) console.log(`  ⚠ Citado como facto mas não está nos dados enviados: ${falta.join(', ')}`);
    console.log();
  }
  for (const aviso of avisos) console.log(`⚠ ${aviso}`);
  if (avisos.length) console.log();
  if (t.perguntas_ao_dono.length) {
    console.log('Perguntas que só tu sabes responder:');
    for (const p of t.perguntas_ao_dono) console.log(`  - ${p}`);
    console.log();
  }
  console.log(`(${modelo}: ${uso.input_tokens} tokens de entrada, ${uso.output_tokens} de saída)\n`);
}
