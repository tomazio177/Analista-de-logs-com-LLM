import Anthropic from '@anthropic-ai/sdk';
import { interfaceLocal, enderecosDaRede, gatewayPadrao } from './rede.js';
import { descobrirAparelhos, sistemaProvavel, macDe } from './descobrir.js';
import { PORTAS, portasAbertas } from './portas.js';
import { emParalelo } from './util.js';
import { compararVarrimentos } from './comparar.js';
import { lerLoginsFalhados, SEM_PERMISSAO } from './eventos.js';
import { interpretar, alertas, nomeDoProcesso, SEM_IP } from './logins.js';
import { recolher, triar, aplicarRegras, alvoDaPergunta, ErroTriagem, MODELO } from './triagem.js';
import { semApoioNosDados } from './verificar.js';
import { carregarEnv } from './env.js';
import {
  guardar, macsConhecidos, ultimoVarrimento, ausenciasPorMac, redeAprovada, aprovarRede,
  guardarLogins, guardarLeitura, coberturaDosLogs, aparelhosPorIp,
  guardarTriagem, ultimaTriagemValida, guardarNota, listarNotas, apagarNota,
} from './historico.js';

// O motor: tudo o que os comandos fazem, sem escrever nada no ecrã. Cada operação devolve um objeto simples,
// { ok: true, … } ou { ok: false, erro: { codigo, mensagem } }, que serve tanto ao terminal como à app.
// Os códigos de erro são estáveis (estão em docs/contrato.md); as mensagens podem mudar.
export const CONTRATO = 1;

const falha = (codigo, mensagem, extra = {}) => ({ ok: false, erro: { codigo, mensagem }, ...extra });
const comServico = (portas) => portas.map((p) => ({ porta: p, servico: PORTAS[p] }));

// --- Varrimento da rede ------------------------------------------------------------------------------------

// `aoProgredir` recebe { fase: 'inicio' | 'descobrir' | 'portas', … } para quem quiser mostrar o andamento.
export async function varrer(db, { aprovar = false, aoProgredir = () => {} } = {}) {
  let local;
  try {
    local = interfaceLocal();
  } catch (erro) {
    return falha('sem-rede', erro.message);
  }
  const { cidr, hosts } = enderecosDaRede(local);

  // Um portátil liga-se a muitas redes, e varrer uma rede alheia é crime. Por isso só se varrem redes aprovadas,
  // reconhecidas pelo MAC do router (o intervalo de endereços repete-se de casa para casa).
  const routerIp = await gatewayPadrao(local);
  const routerMac = routerIp && (await macDe(routerIp));
  const base = { rede: cidr, interface: local.nome, ip_deste_pc: local.ip, router: routerIp ? { ip: routerIp, mac: routerMac } : null };
  aoProgredir({ fase: 'inicio', ...base });
  if (!routerMac) {
    return falha('sem-router', 'Não consegui saber qual é o router desta rede, por isso não sei se é uma rede aprovada. Não varri nada.', base);
  }
  let aprovadaAgora = false;
  if (!redeAprovada(db, routerMac)) {
    if (!aprovar) return falha('rede-por-aprovar', `Rede nova: router ${routerIp} (${routerMac}). Ainda não está aprovada, por isso não varri nada.`, base);
    aprovarRede(db, routerMac, cidr);
    aprovadaAgora = true;
  }

  const rede = { cidr, router: routerMac };
  const aparelhos = await descobrirAparelhos(local, hosts, (feitos, total) => aoProgredir({ fase: 'descobrir', feitos, total }));
  aoProgredir({ fase: 'portas', aparelhos: aparelhos.length, portas: Object.keys(PORTAS).length });
  await emParalelo(aparelhos, 8, async (a) => {
    a.portas = await portasAbertas(a.ip);
  });

  const anterior = ultimoVarrimento(db, rede);
  const mudancas = compararVarrimentos(anterior, aparelhos, macsConhecidos(db), ausenciasPorMac(db, rede));
  guardar(db, rede, aparelhos);

  return {
    ok: true,
    ...base,
    aprovada_agora: aprovadaAgora,
    anterior: anterior?.data ?? null,
    aparelhos: aparelhos.map((a) => ({
      ip: a.ip, mac: a.mac, nome: a.nome, este_pc: a.proprio, ttl: a.ttl, sistema: sistemaProvavel(a.ttl), portas: comServico(a.portas),
    })),
    mudancas: mudancas.map((m) => ({
      tipo: m.tipo,
      ip: m.aparelho.ip,
      mac: m.aparelho.mac,
      nome: m.aparelho.nome ?? null,
      ...(m.de && { ip_anterior: m.de }),
      ...(m.portas && { portas: comServico(m.portas) }),
      ...(m.mesmoNomeQue && { mesmo_nome_que: m.mesmoNomeQue }),
    })),
  };
}

// --- Logins falhados do Windows ----------------------------------------------------------------------------

export async function lerLogs(db, { dias = 7 } = {}) {
  if (!Number.isInteger(dias) || dias < 1 || dias > 365) return falha('dias-invalidos', 'Os dias têm de ser um número inteiro de 1 a 365.');
  if (process.platform !== 'win32') return falha('nao-windows', 'Esta parte lê o registo de eventos do Windows: só funciona no Windows.');

  let lido;
  try {
    lido = await lerLoginsFalhados(dias);
  } catch (erro) {
    if (erro.code === SEM_PERMISSAO) return falha('sem-permissao', 'O Windows não deixa ler o registo de Segurança sem privilégios de administrador.');
    throw erro;
  }

  const logins = lido.eventos.map(interpretar).sort((a, b) => b.quando.localeCompare(a.quando));
  const novos = guardarLogins(db, logins);
  guardarLeitura(db, new Date().toISOString(), lido.maisAntigo);
  const conhecidos = aparelhosPorIp(db);
  const aparelhoDe = (origem) => conhecidos.get(origem) ?? null;
  const { buracos } = coberturaDosLogs(db, new Date(Date.now() - dias * 86_400_000).toISOString());

  return {
    ok: true,
    dias,
    registo_mais_antigo: lido.maisAntigo,
    total: logins.length,
    novos: novos.size,
    logins: logins.map((l) => {
      const origem = l.ip ?? (l.pelaRede ? SEM_IP : 'este PC');
      return {
        quando: l.quando, utilizador: l.utilizador, tipo: l.tipoTexto, motivo: l.motivo,
        origem, aparelho_de_origem: aparelhoDe(origem), processo: nomeDoProcesso(l.processo), novo: novos.has(l.id),
      };
    }),
    alertas: alertas(logins).map((a) => ({ ...a, aparelho_de_origem: aparelhoDe(a.origem) })),
    periodos_possivelmente_incompletos: buracos,
  };
}

// --- Triagem com o LLM -------------------------------------------------------------------------------------

export function dadosDaTriagem(db) {
  const recolhido = recolher(db);
  return recolhido ? { ok: true, dados: recolhido.dados } : semVarrimentos();
}

const semVarrimentos = () => falha('sem-varrimentos', 'Ainda não há varrimentos guardados. Faz primeiro um varrimento.');

function erroDaTriagem(erro) {
  if (erro instanceof ErroTriagem) return { codigo: erro.codigo, mensagem: erro.message };
  if (erro instanceof Anthropic.AuthenticationError) return { codigo: 'chave-recusada', mensagem: 'A chave da API foi recusada. Confirma o valor no .env.' };
  if (erro instanceof Anthropic.RateLimitError) return { codigo: 'limite-de-pedidos', mensagem: 'Limite de pedidos atingido. Tenta daqui a um minuto.' };
  if (erro instanceof Anthropic.APIConnectionError) return { codigo: 'sem-ligacao', mensagem: 'Não consegui ligar à API. Confirma a ligação à internet.' };
  if (erro instanceof Anthropic.APIError) return { codigo: 'erro-da-api', mensagem: `Erro da API (${erro.status}): ${erro.message}` };
  return null;
}

// `cliente` é para os testes: sem ele, usa a API a sério com a chave do .env.
export async function fazerTriagem(db, { cliente } = {}) {
  const recolhido = recolher(db);
  if (!recolhido) return semVarrimentos();
  const { varrimentoId, dados, ancoras } = recolhido;

  if (!cliente) {
    const problema = carregarEnv();
    if (!process.env.ANTHROPIC_API_KEY) {
      return falha('sem-chave', 'Falta a chave da API. Copia o .env.example para .env e preenche a ANTHROPIC_API_KEY.', { detalhe: problema });
    }
    cliente = new Anthropic();
  }

  let resultado;
  try {
    resultado = await triar(dados, cliente);
  } catch (erro) {
    const e = erroDaTriagem(erro);
    if (!e) throw erro;
    // As falhas também ficam guardadas: uma recusa ou uma resposta fora do esquema é informação sobre o modelo.
    guardarTriagem(db, { varrimentoId, modelo: MODELO, entrada: dados, resposta: erro.bruto ?? null, erro: e.mensagem });
    return falha(e.codigo, e.mensagem);
  }

  const triagemId = guardarTriagem(db, { varrimentoId, modelo: resultado.modelo, entrada: dados, resposta: resultado.bruto, erro: null, ancoras });
  return apresentarTriagem(db, {
    id: triagemId, data: new Date().toISOString(), modelo: resultado.modelo, dados, resposta: resultado.triagem,
    ancoras, uso: { entrada: resultado.uso?.input_tokens ?? null, saida: resultado.uso?.output_tokens ?? null },
  });
}

// A triagem tal como a app a mostra: com as regras aplicadas e as perguntas a dizer se já têm resposta.
// Serve tanto à triagem acabada de fazer como à última guardada, que a app abre sem gastar a API.
function apresentarTriagem(db, { id, data, modelo, dados, resposta, ancoras, uso }) {
  const { triagem, avisos } = aplicarRegras(dados, resposta);
  return {
    ok: true,
    triagem_id: id,
    data,
    modelo,
    uso,
    nivel_geral: triagem.nivel_geral,
    resumo: triagem.resumo,
    achados: triagem.achados.map((a) => ({ ...a, factos_sem_apoio: semApoioNosDados(a.nos_dados, dados) })),
    avisos,
    perguntas: perguntasDe(db, { id, resposta: JSON.stringify(resposta), ancoras: JSON.stringify(ancoras) }),
  };
}

// A última triagem guardada, sem chamar a API.
export function ultimaTriagem(db) {
  const t = ultimaTriagemValida(db);
  if (!t) return falha('sem-triagem', 'Ainda não há nenhuma triagem com resposta. Faz primeiro uma triagem.');
  return apresentarTriagem(db, {
    id: t.id, data: t.data, modelo: t.modelo, dados: JSON.parse(t.entrada), resposta: JSON.parse(t.resposta),
    ancoras: t.ancoras && JSON.parse(t.ancoras), uso: { entrada: null, saida: null },
  });
}

// --- Respostas do dono -------------------------------------------------------------------------------------

function perguntasDe(db, triagem) {
  const perguntas = JSON.parse(triagem.resposta).perguntas_ao_dono ?? [];
  const ancoras = triagem.ancoras && JSON.parse(triagem.ancoras);
  const respondidas = new Set(listarNotas(db).filter((n) => n.triagem_id === triagem.id).map((n) => n.pergunta));
  return perguntas.map((p, i) => {
    const alvo = alvoDaPergunta(p, ancoras);
    return {
      n: i + 1,
      pergunta: alvo.pergunta,
      sobre: alvo.alvoTipo === 'geral' ? null : { tipo: alvo.alvoTipo, alvo: alvo.alvo },
      respondida: respondidas.has(alvo.pergunta),
    };
  });
}

export function perguntas(db) {
  const triagem = ultimaTriagemValida(db);
  if (!triagem) return falha('sem-triagem', 'Ainda não há nenhuma triagem com resposta. Faz primeiro uma triagem.');
  return { ok: true, triagem: { id: triagem.id, data: triagem.data }, perguntas: perguntasDe(db, triagem) };
}

export function responder(db, { n, texto }) {
  const r = perguntas(db);
  if (!r.ok) return r;
  const p = r.perguntas[n - 1];
  if (!p) return falha('pergunta-inexistente', `A última triagem não tem a pergunta n.º ${n}.`);
  if (!texto?.trim()) return falha('resposta-vazia', 'A resposta está vazia.');
  const id = guardarNota(db, { alvoTipo: p.sobre?.tipo ?? 'geral', alvo: p.sobre?.alvo ?? null, pergunta: p.pergunta, texto: texto.trim(), triagemId: r.triagem.id });
  return { ok: true, resposta_id: id };
}

export function respostas(db) {
  return {
    ok: true,
    respostas: listarNotas(db).map((n) => ({
      id: n.id, data: n.data, sobre: n.alvo_tipo === 'geral' ? null : { tipo: n.alvo_tipo, alvo: n.alvo }, pergunta: n.pergunta, texto: n.texto,
    })),
  };
}

export function apagarResposta(db, id) {
  return apagarNota(db, id) ? { ok: true } : falha('resposta-inexistente', `Não há nenhuma resposta com o n.º ${id}.`);
}

// --- Tudo de seguida ---------------------------------------------------------------------------------------

// O que se corre de hora a hora (ou com o botão da app): varrer, ler os logins e pedir a triagem.
// Um passo que falha não impede os outros: sem privilégios não há logins novos, mas a triagem continua a ter o resto.
export async function vigiar(db, { aprovar = false, aoProgredir, cliente } = {}) {
  const varrimento = await varrer(db, { aprovar, aoProgredir });
  const logins = await lerLogs(db);
  const triagem = await fazerTriagem(db, { cliente });
  return { ok: triagem.ok, varrimento, logins, triagem };
}
