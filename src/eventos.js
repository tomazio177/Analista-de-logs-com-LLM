import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { limpar } from './util.js';

const correr = promisify(execFile);

// Um ataque de força bruta gera milhares de eventos por hora; não vale a pena ler mais do que isto.
const MAX_EVENTOS = 5000;

export const TIPOS_LOGIN = {
  2: 'teclado',
  3: 'rede',
  4: 'tarefa agendada',
  5: 'serviço',
  7: 'desbloqueio do ecrã',
  8: 'rede (texto simples)',
  9: 'credenciais novas',
  10: 'remoto (RDP)',
  11: 'teclado (cache)',
};

// Logins que vêm de outra máquina. Os outros são alguém (ou algo) neste próprio PC.
export const TIPOS_REMOTOS = new Set([3, 8, 10]);

// O Status diz "falhou"; o SubStatus, quando existe, diz porquê.
const MOTIVOS = {
  '0xc000006a': 'password errada',
  '0xc0000064': 'utilizador não existe',
  '0xc000006d': 'credenciais inválidas',
  '0xc0000234': 'conta bloqueada',
  '0xc0000072': 'conta desativada',
  '0xc0000193': 'conta expirada',
  '0xc0000071': 'password expirada',
  '0xc0000224': 'tem de mudar a password',
  '0xc000006f': 'fora do horário permitido',
  '0xc0000070': 'máquina não autorizada',
  '0xc000015b': 'tipo de login não autorizado',
  '0xc0000133': 'relógio dessincronizado',
};

// Lê os logins falhados (evento 4625) do registo de Segurança do Windows desde `desde`.
// Ler esse registo exige administrador ou pertencer ao grupo "Event Log Readers" — sem isso devolve 'sem-permissao'.
export async function lerLoginsFalhados(desde) {
  if (process.platform !== 'win32') return { estado: 'indisponivel', grupos: [] };

  const consulta = `*[System[(EventID=4625) and TimeCreated[@SystemTime>='${desde.toISOString()}']]]`;
  const args = ['qe', 'Security', `/q:${consulta}`, '/f:xml', '/rd:true', `/c:${MAX_EVENTOS}`];
  try {
    const { stdout } = await correr('wevtutil', args, { windowsHide: true, encoding: 'buffer', maxBuffer: 64 * 1024 * 1024 });
    const eventos = lerEventosXml(descodificarSaida(stdout));
    return { estado: 'ok', grupos: agruparLoginsFalhados(eventos), total: eventos.length, truncado: eventos.length >= MAX_EVENTOS };
  } catch (erro) {
    // O wevtutil sai com o código de erro do Windows: 5 é "acesso negado", em qualquer língua.
    if (erro.code === 5) return { estado: 'sem-permissao', grupos: [] };
    // Com encoding 'buffer', o stderr é sempre um Buffer, mesmo vazio (ex.: wevtutil não encontrado).
    const stderr = erro.stderr?.length ? limpar(descodificarSaida(erro.stderr), 200) : '';
    return { estado: 'erro', grupos: [], motivo: stderr || limpar(erro.message, 200) };
  }
}

// Conforme a versão e a consola, o wevtutil pode escrever em UTF-16; o primeiro carácter é sempre "<".
function descodificarSaida(buffer) {
  return buffer.length > 1 && (buffer[1] === 0 || (buffer[0] === 0xff && buffer[1] === 0xfe))
    ? buffer.toString('utf16le')
    : buffer.toString('utf8');
}

const ENTIDADES = { lt: '<', gt: '>', amp: '&', quot: '"', apos: "'" };
const descodificarXml = (s) =>
  s.replace(/&(#x[0-9a-f]+|#\d+|\w+);/gi, (original, e) => {
    if (e[0] !== '#') return ENTIDADES[e] ?? original;
    return String.fromCodePoint(e[1].toLowerCase() === 'x' ? parseInt(e.slice(2), 16) : Number(e.slice(1)));
  });

// O wevtutil devolve os eventos em XML colados uns aos outros, sem elemento raiz.
// O formato é fixo e os campos têm nome (não dependem da língua do Windows), por isso chegam expressões regulares.
export function lerEventosXml(xml) {
  const eventos = [];
  for (const [bloco] of xml.matchAll(/<Event[\s>][\s\S]*?<\/Event>/g)) {
    const campos = {};
    for (const [, nome, valor = ''] of bloco.matchAll(/<Data Name=['"](\w+)['"]\s*(?:\/>|>([^<]*)<\/Data>)/g)) {
      campos[nome] = descodificarXml(valor).trim();
    }
    const vazio = (v) => (!v || v === '-' ? null : v);
    const tipoLogin = Number(campos.LogonType);
    const status = (campos.Status ?? '').toLowerCase();
    const subStatus = (campos.SubStatus ?? '').toLowerCase();
    const codigo = vazio(subStatus) && subStatus !== '0x0' ? subStatus : status;

    eventos.push({
      data: /<TimeCreated SystemTime=['"]([^'"]+)['"]/.exec(bloco)?.[1] ?? null,
      // O nome de utilizador e o da máquina são escolhidos por quem tenta entrar.
      utilizador: limpar(vazio(campos.TargetUserName), 64),
      origem: normalizarOrigem(campos.IpAddress, tipoLogin),
      tipoLogin,
      motivo: MOTIVOS[codigo] ?? codigo,
      estacao: limpar(vazio(campos.WorkstationName), 64),
    });
  }
  return eventos;
}

// Sem IP não quer dizer local: com RDP por NLA, ou NTLM por SMB, o Windows regista logins de rede
// (tipo 3) sem endereço de origem. Nesses tipos, a origem fica 'desconhecida' e continua a contar como remota.
function normalizarOrigem(ip, tipoLogin) {
  if (ip === '127.0.0.1' || ip === '::1') return 'local';
  if (!ip || ip === '-') return TIPOS_REMOTOS.has(tipoLogin) ? 'desconhecida' : 'local';
  return limpar(ip.replace(/^::ffff:/i, ''), 64);
}

const contar = (mapa, chave) => mapa.set(chave, (mapa.get(chave) ?? 0) + 1);
const topo = (mapa, n) =>
  [...mapa].sort((a, b) => b[1] - a[1]).slice(0, n).map(([valor, tentativas]) => ({ valor, tentativas }));

// Agrupa por origem e tipo de login: quem ataca é a origem; os nomes de utilizador são o que vai variando.
// Assim, uma tentativa de força bruta (1 utilizador, muitas vezes) e um "password spraying"
// (muitos utilizadores, poucas vezes cada) ficam ambos num só grupo, visíveis como tal.
export function agruparLoginsFalhados(eventos) {
  const grupos = new Map();
  for (const e of eventos) {
    const chave = `${e.origem}|${e.tipoLogin}`;
    let g = grupos.get(chave);
    if (!g) {
      g = { origem: e.origem, tipoLogin: e.tipoLogin, tentativas: 0, primeira: e.data, ultima: e.data, utilizadores: new Map(), motivos: new Map(), estacoes: new Map() };
      grupos.set(chave, g);
    }
    g.tentativas++;
    if (e.data < g.primeira) g.primeira = e.data;
    if (e.data > g.ultima) g.ultima = e.data;
    contar(g.utilizadores, e.utilizador ?? '(vazio)');
    contar(g.motivos, e.motivo);
    if (e.estacao) contar(g.estacoes, e.estacao);
  }

  return [...grupos.values()]
    .sort((a, b) => b.tentativas - a.tentativas)
    .map((g, i) => ({
      id: `l${i + 1}`,
      tipo: 'logins-falhados',
      origem: g.origem,
      tipoLogin: g.tipoLogin,
      tentativas: g.tentativas,
      primeira: g.primeira,
      ultima: g.ultima,
      utilizadores: topo(g.utilizadores, 5),
      utilizadoresDistintos: g.utilizadores.size,
      motivos: topo(g.motivos, 3),
      estacoes: topo(g.estacoes, 3).map((e) => e.valor),
    }));
}
