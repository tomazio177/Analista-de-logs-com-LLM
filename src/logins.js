// Tipo de logon (campo LogonType do evento 4625): por onde a tentativa entrou.
const TIPOS = {
  2: 'teclado',
  3: 'rede (partilhas/SMB)',
  4: 'tarefa agendada',
  5: 'serviço',
  7: 'desbloqueio do ecrã',
  8: 'rede (texto simples)',
  9: 'credenciais alternativas',
  10: 'remoto (RDP)',
  11: 'teclado (credenciais em cache)',
};
const PELA_REDE = new Set([3, 8, 10]);

// Porque falhou (campo SubStatus, ou Status quando o SubStatus vem vazio).
const MOTIVOS = {
  '0xc000006a': 'palavra-passe errada',
  '0xc0000064': 'utilizador não existe',
  '0xc000006d': 'utilizador ou palavra-passe errados',
  '0xc0000234': 'conta bloqueada',
  '0xc0000072': 'conta desativada',
  '0xc000006f': 'fora do horário permitido',
  '0xc0000070': 'aparelho não autorizado',
  '0xc0000071': 'palavra-passe expirada',
  '0xc0000193': 'conta expirada',
  '0xc0000224': 'tem de mudar a palavra-passe',
  '0xc000015b': 'tipo de login não permitido',
};

const LOCAL = new Set(['127.0.0.1', '::1']);
const limpar = (v) => (!v || v === '-' ? null : v);

// Transforma o evento em bruto (campos com os nomes do Windows) num login falhado legível.
export function interpretar({ id, quando, campos }) {
  const sub = campos.SubStatus?.toLowerCase();
  const codigo = (sub && sub !== '0x0' ? sub : campos.Status?.toLowerCase()) ?? null;
  const tipo = Number(campos.LogonType);
  const ip = limpar(campos.IpAddress)?.replace(/^::ffff:/i, '') ?? null;

  return completar({
    id,
    quando,
    utilizador: limpar(campos.TargetUserName),
    tipo,
    codigo,
    ip: ip && !LOCAL.has(ip) ? ip : null,
    estacao: limpar(campos.WorkstationName),
    processo: limpar(campos.ProcessName),
  });
}

// Acrescenta os campos legíveis. Serve tanto para eventos acabados de ler como para linhas da base de dados.
export function completar(l) {
  return {
    ...l,
    tipoTexto: TIPOS[l.tipo] ?? `tipo ${l.tipo}`,
    pelaRede: PELA_REDE.has(l.tipo),
    motivo: MOTIVOS[l.codigo] ?? `código ${l.codigo}`,
  };
}

// Regras fixas: decidem o que é grave. O LLM vem depois, para explicar — não para decidir.
export const RAJADA = { tentativas: 5, minutos: 10 };

const origemDe = (l) => l.ip ?? 'este PC';
export const nomeDoProcesso = (caminho) => caminho?.split('\\').pop() ?? null;

// Uma pessoa a errar a palavra-passe leva vários segundos entre tentativas; um programa a repetir uma credencial, menos de 2.
export const RITMO_DE_MAQUINA = 2;

function intervaloMediano(janela) {
  const intervalos = janela.slice(1).map((l, i) => (Date.parse(l.quando) - Date.parse(janela[i].quando)) / 1000);
  const ordenados = intervalos.toSorted((a, b) => a - b);
  return Math.round(ordenados[Math.floor(ordenados.length / 2)] * 10) / 10;
}
const agrupar = (lista, chave) => Map.groupBy(lista, chave);

export function alertas(logins) {
  const resultado = [];

  // Muitas tentativas seguidas da mesma origem: alguém (ou algum programa) a adivinhar palavras-passe.
  for (const [origem, lista] of agrupar(logins, origemDe)) {
    const ordenada = lista.toSorted((a, b) => a.quando.localeCompare(b.quando));
    let inicio = 0;
    let pior = null;
    for (let fim = 0; fim < ordenada.length; fim++) {
      while (Date.parse(ordenada[fim].quando) - Date.parse(ordenada[inicio].quando) > RAJADA.minutos * 60_000) inicio++;
      const n = fim - inicio + 1;
      if (n >= RAJADA.tentativas && n > (pior?.n ?? 0)) pior = { n, inicio, fim };
    }
    if (pior) {
      const janela = ordenada.slice(pior.inicio, pior.fim + 1);
      resultado.push({
        tipo: 'rajada',
        origem,
        n: pior.n,
        de: janela[0].quando,
        ate: janela.at(-1).quando,
        segundosEntreTentativas: intervaloMediano(janela),
        processos: [...new Set(janela.map((l) => nomeDoProcesso(l.processo) ?? 'desconhecido'))],
      });
    }
  }

  // Tentativas vindas de outro aparelho da rede: é a ligação com a fase 1 (porta 445 aberta → alguém tentou usá-la?).
  for (const [ip, lista] of agrupar(logins.filter((l) => l.pelaRede && l.ip), (l) => l.ip)) {
    resultado.push({ tipo: 'pela-rede', origem: ip, n: lista.length, utilizadores: [...new Set(lista.map((l) => l.utilizador))] });
  }

  // Nomes que não existem neste PC: típico de quem tenta "admin", "administrator", "test"…
  const inexistentes = logins.filter((l) => l.codigo === '0xc0000064');
  if (inexistentes.length) {
    resultado.push({ tipo: 'utilizador-inexistente', n: inexistentes.length, utilizadores: [...new Set(inexistentes.map((l) => l.utilizador))] });
  }

  return resultado;
}
