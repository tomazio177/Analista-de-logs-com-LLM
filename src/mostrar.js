import { parseArgs } from 'node:util';
import { macPrivado } from './rede.js';
import { RAJADA, SEM_IP } from './logins.js';
import { CONTRATO } from './motor.js';

// Tudo o que vai para o terminal. O motor devolve objetos; aqui decide-se como as pessoas os leem.
// Com --json não se escreve nada disto: sai só o documento do contrato (docs/contrato.md).

// Escreve o resultado e acerta o código de saída: 0 se correu bem, 1 se não.
export function emitir(resultado, { json, mostrar }) {
  if (json) process.stdout.write(JSON.stringify({ contrato: CONTRATO, ...resultado }) + '\n');
  else if (resultado.erro) mostrarFalha(resultado);
  else mostrar(resultado); // inclui o vigiar, que mostra os erros de cada passo

  process.exitCode = resultado.ok ? 0 : 1;
}

// As opções de cada comando. Uma opção mal escrita sai como erro do contrato, e não como um rasto do Node.
export function lerOpcoes(config) {
  try {
    return parseArgs(config);
  } catch (erro) {
    emitir({ ok: false, erro: { codigo: 'opcao-invalida', mensagem: `Opção inválida: ${erro.message}` } }, { json: process.argv.includes('--json') });
    process.exit(1);
  }
}

// Com --json, o andamento vai para o stderr, um objeto por linha: o stdout fica só com o resultado.
export function progresso(json) {
  if (json) return (e) => process.stderr.write(JSON.stringify({ progresso: e }) + '\n');
  return (e) => {
    if (e.fase === 'inicio') console.log(`\nAnalista de logs com LLM — rede ${e.rede} (interface "${e.interface}", este PC é ${e.ip_deste_pc})`);
    if (e.fase === 'descobrir') process.stdout.write(`\rA procurar aparelhos… ${e.feitos}/${e.total}${e.feitos === e.total ? '\n' : ''}`);
    if (e.fase === 'portas') console.log(`Encontrados ${e.aparelhos} aparelhos. A testar ${e.portas} portas em cada…\n`);
  };
}

// O que fazer a seguir, no terminal. A app terá os seus botões; os códigos são os mesmos.
const DICAS = {
  'rede-por-aprovar': 'Se a rede é tua, ou tens autorização de quem manda nela, aprova-a uma vez acrescentando --aprovar:\n  npm run scan -- --aprovar',
  'sem-permissao': 'Abre o PowerShell como administrador (botão direito → "Executar como administrador") e corre outra vez.',
  'sem-chave': 'O .env já está no .gitignore, por isso não vai parar ao GitHub.',
  'sem-varrimentos': 'Corre primeiro: npm run scan',
  'sem-triagem': 'Corre primeiro: npm run triagem',
  'dias-invalidos': 'Uso: npm run logs -- [dias]   (de 1 a 365; por omissão, 7)',
};

export function mostrarFalha(r) {
  if (r.detalhe) console.error(`\n${r.detalhe}`);
  console.error(`\n${r.erro.mensagem}`);
  if (DICAS[r.erro.codigo]) console.error(DICAS[r.erro.codigo]);
  console.error();
}

const servicos = (portas) => portas.map((p) => `${p.porta} ${p.servico}`).join(', ');
const quando = (iso) => new Date(iso).toLocaleString('pt-PT', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit' });

// --- Varrimento --------------------------------------------------------------------------------------------

export function mostrarVarrimento(r) {
  if (r.aprovada_agora) console.log(`Rede aprovada (router ${r.router.ip}, ${r.router.mac}).\n`);
  const colunas = (...c) => c[0].padEnd(16) + c[1].padEnd(20) + c[2].padEnd(28) + c[3].padEnd(26) + c[4];
  console.log(colunas('IP', 'MAC', 'Nome', 'Sistema (pelo TTL)', 'Portas abertas'));
  console.log('─'.repeat(116));
  for (const a of r.aparelhos) {
    const nome = (a.este_pc ? '(este PC) ' : '') + (a.nome ?? '—');
    const sistema = a.ttl == null ? '— (não respondeu ao ping)' : `${a.sistema} (${a.ttl})`;
    console.log(colunas(a.ip, a.mac, nome.slice(0, 27), sistema, servicos(a.portas) || '—'));
  }

  if (!r.anterior) {
    console.log('\nPrimeiro varrimento desta rede: fica guardado como linha de base.');
    console.log('Corre outra vez mais tarde para ver o que mudou.\n');
    return;
  }
  const desde = new Date(r.anterior).toLocaleString('pt-PT');
  if (!r.mudancas.length) {
    console.log(`\nNada mudou desde o último varrimento (${desde}).\n`);
    return;
  }
  const porqueNovo = (m) => {
    if (m.mesmo_nome_que) return `mesmo nome que ${m.mesmo_nome_que}: provavelmente o mesmo aparelho com MAC novo`;
    if (macPrivado(m.mac)) return `${m.nome ?? 'sem nome'} — MAC privado, provavelmente telemóvel ou tablet`;
    return `${m.nome ?? 'sem nome'} — nunca visto antes`;
  };
  const descrever = {
    novo: (m) => `+ NOVO          ${m.ip}  ${m.mac}  ${porqueNovo(m)}`,
    voltou: (m) => `↺ VOLTOU        ${m.ip}  ${m.mac}  (tinha desaparecido)`,
    desapareceu: (m) => `− SAIU          ${m.ip}  ${m.mac}`,
    'mudou-ip': (m) => `~ IP MUDOU      ${m.mac}  ${m.ip_anterior} → ${m.ip}`,
    'portas-abertas': (m) => `! PORTA ABRIU   ${m.ip}  ${servicos(m.portas)}`,
    'portas-fechadas': (m) => `· PORTA FECHOU  ${m.ip}  ${servicos(m.portas)}`,
  };
  console.log(`\nMudanças desde o último varrimento (${desde}):`);
  for (const m of r.mudancas) console.log('  ' + descrever[m.tipo](m));
  console.log();
}

// --- Logins falhados ---------------------------------------------------------------------------------------

function descreverOrigem(origem, aparelho) {
  if (origem === 'este PC' || origem === SEM_IP) return origem;
  return aparelho ? `${origem} (${aparelho.nome ?? aparelho.mac})` : `${origem} (aparelho que nunca apareceu num varrimento)`;
}

export function mostrarLogs(r) {
  console.log(`\nLogins falhados no Windows — últimos ${r.dias} dias: ${r.total} eventos, ${r.novos} novos desde a última leitura\n`);
  for (const b of r.periodos_possivelmente_incompletos) {
    console.log(`⚠ De ${quando(b.de)} a ${quando(b.ate)} não há leitura que garanta os logins falhados: o registo de Segurança encheu e o Windows já apagou esse período.`);
    console.log('  Se houve tentativas nesse intervalo, só estão na base de dados se uma versão anterior do programa as leu a tempo.\n');
  }

  if (!r.logins.length) {
    console.log('Nenhum. Para confirmar que o registo está a funcionar, provoca um de propósito:');
    console.log('  runas /user:naoexiste cmd      (escreve qualquer palavra-passe)');
    console.log('e corre outra vez. Se continuar sem aparecer nada, a auditoria de logins pode estar desligada:');
    console.log('  auditpol /get /subcategory:{0CCE9215-69AE-11D9-BED3-505054503030}\n');
    return;
  }

  const MAXIMO = 30;
  const colunas = (...c) => c[0].padEnd(2) + c[1].padEnd(18) + c[2].padEnd(16) + c[3].padEnd(22) + c[4].padEnd(30) + c[5].padEnd(22) + c[6];
  console.log(colunas('', 'QUANDO', 'UTILIZADOR', 'TIPO', 'ORIGEM', 'PROCESSO', 'MOTIVO'));
  console.log('─'.repeat(130));
  for (const l of r.logins.slice(0, MAXIMO)) {
    const origem = descreverOrigem(l.origem, l.aparelho_de_origem);
    console.log(colunas(l.novo ? '*' : '', quando(l.quando), (l.utilizador ?? '—').slice(0, 15), l.tipo.slice(0, 21), origem.slice(0, 29), (l.processo ?? '—').slice(0, 21), l.motivo));
  }
  if (r.logins.length > MAXIMO) console.log(`  … e mais ${r.logins.length - MAXIMO} mais antigos.`);
  if (r.novos) console.log('\n* = novo desde a última leitura');

  const descrever = {
    rajada: (a) => {
      const minutos = Math.max(1, Math.round((Date.parse(a.ate) - Date.parse(a.de)) / 60_000));
      const ritmo = a.ritmoDeMaquina
        ? `uma a cada ${a.segundosEntreTentativas} s: ritmo de programa, não de pessoa`
        : `uma a cada ${Math.round(a.segundosEntreTentativas)} s`;
      return (
        `! RAJADA        ${a.n} tentativas em ${minutos} min, a partir de ${descreverOrigem(a.origem, a.aparelho_de_origem)} (${quando(a.de)})\n` +
        `                ${ritmo}; pedidas por: ${a.processos.join(', ')}`
      );
    },
    'pela-rede': (a) => `! PELA REDE     ${a.n} tentativas vindas de ${descreverOrigem(a.origem, a.aparelho_de_origem)}, com: ${a.utilizadores.join(', ')}`,
    'utilizador-inexistente': (a) =>
      `· NOMES FALSOS  ${a.n} tentativas a partir de ${descreverOrigem(a.origem, a.aparelho_de_origem)} com utilizadores que não existem neste PC: ${a.utilizadores.join(', ')}`,
  };
  if (r.alertas.length) {
    console.log(`\nAlertas (rajada = ${RAJADA.tentativas} ou mais tentativas em ${RAJADA.minutos} minutos):`);
    for (const a of r.alertas) console.log('  ' + descrever[a.tipo](a));
  } else {
    console.log('\nSem alertas: nenhuma rajada, nada vindo de outros aparelhos da rede.');
  }
  console.log();
}

// --- Triagem -----------------------------------------------------------------------------------------------

export function mostrarTriagem(r) {
  const NIVEL = { tranquilo: '✓ TRANQUILO', atencao: '! ATENÇÃO', urgente: '!! URGENTE' };
  const ORDEM = { alta: 0, media: 1, baixa: 2, info: 3 };
  const GRAVIDADE = { alta: 'ALTA ', media: 'MÉDIA', baixa: 'BAIXA', info: 'INFO ' };

  console.log(`\n${NIVEL[r.nivel_geral]}\n\n${r.resumo}\n`);
  for (const a of r.achados.toSorted((x, y) => ORDEM[x.gravidade] - ORDEM[y.gravidade])) {
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
    if (a.factos_sem_apoio.length) console.log(`  ⚠ Citado como facto mas não está nos dados enviados: ${a.factos_sem_apoio.join(', ')}`);
    console.log();
  }
  for (const aviso of r.avisos) console.log(`⚠ ${aviso}`);
  if (r.avisos.length) console.log();
  if (r.perguntas.length) {
    console.log('Perguntas que só tu sabes responder:');
    for (const p of r.perguntas) console.log(`  ${p.n}. ${p.pergunta}`);
    console.log('\nPara responder (a triagem seguinte já não pergunta o mesmo): npm run responder\n');
  }
  console.log(`(${r.modelo}: ${r.uso.entrada} tokens de entrada, ${r.uso.saida} de saída)\n`);
}

// --- Respostas do dono -------------------------------------------------------------------------------------

function descreverAlvo(sobre) {
  if (!sobre) return 'geral';
  if (sobre.tipo === 'aparelho') return `aparelho ${sobre.alvo}`;
  const [tipo, origem, de] = sobre.alvo.split('|');
  return `alerta ${tipo} de ${origem}, a ${quando(de)}`;
}

export function mostrarRespostas(r) {
  if (!r.respostas.length) console.log('\nAinda não há respostas guardadas. Depois de uma triagem: npm run responder\n');
  for (const n of r.respostas) {
    console.log(`\n${n.id}. [${descreverAlvo(n.sobre)}] ${quando(n.data)}`);
    if (n.pergunta) console.log(`   P: ${n.pergunta}`);
    console.log(`   R: ${n.texto}`);
  }
  if (r.respostas.length) console.log('\nPara apagar uma: npm run notas -- --apagar <n.º>\n');
}

export function mostrarPerguntas(r) {
  console.log(`\nPerguntas da triagem de ${quando(r.triagem.data)}:`);
  for (const p of r.perguntas) console.log(`  ${p.n}. ${p.pergunta}${p.respondida ? '   (respondida)' : ''}`);
  console.log();
}

// --- Tudo de seguida ---------------------------------------------------------------------------------------

export function mostrarVigia(r) {
  const passo = (resultado, mostrar) => (resultado.ok ? mostrar(resultado) : mostrarFalha(resultado));
  passo(r.varrimento, mostrarVarrimento);
  // Fora do Windows não há logins para ler: não vale a pena um erro em cada vigia.
  if (r.logins.erro?.codigo !== 'nao-windows') passo(r.logins, mostrarLogs);
  passo(r.triagem, mostrarTriagem);
}
