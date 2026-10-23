import { lerLoginsFalhados, SEM_PERMISSAO } from './eventos.js';
import { interpretar, alertas, RAJADA } from './logins.js';
import { abrir, guardarLogins, aparelhosPorIp } from './historico.js';

const dias = Number(process.argv[2] ?? 7);
if (!Number.isInteger(dias) || dias < 1 || dias > 365) {
  console.error('Uso: npm run logs -- [dias]   (de 1 a 365; por omissão, 7)');
  process.exit(1);
}
if (process.platform !== 'win32') {
  console.error('Esta parte lê o registo de eventos do Windows: só funciona no Windows.');
  process.exit(1);
}

let brutos;
try {
  brutos = await lerLoginsFalhados(dias);
} catch (erro) {
  if (erro.code === SEM_PERMISSAO) {
    console.error('\nO Windows não deixa ler o registo de Segurança sem privilégios.');
    console.error('Abre o PowerShell como administrador (botão direito → "Executar como administrador") e corre outra vez.\n');
    process.exit(1);
  }
  throw erro;
}

const logins = brutos.map(interpretar).sort((a, b) => b.quando.localeCompare(a.quando));
const db = abrir();
const novos = guardarLogins(db, logins);
const aparelhos = aparelhosPorIp(db);

const quando = (iso) => new Date(iso).toLocaleString('pt-PT', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit' });
const descreverOrigem = (origem) => {
  if (origem === 'este PC') return 'este PC';
  const a = aparelhos.get(origem);
  return a ? `${origem} (${a.nome ?? a.mac})` : `${origem} (aparelho que nunca apareceu num varrimento)`;
};

console.log(`\nLogins falhados no Windows — últimos ${dias} dias: ${logins.length} eventos, ${novos.size} novos desde a última leitura\n`);

if (!logins.length) {
  console.log('Nenhum. Para confirmar que o registo está a funcionar, provoca um de propósito:');
  console.log('  runas /user:naoexiste cmd      (escreve qualquer palavra-passe)');
  console.log('e corre outra vez. Se continuar sem aparecer nada, a auditoria de logins pode estar desligada:');
  console.log('  auditpol /get /subcategory:{0CCE9215-69AE-11D9-BED3-505054503030}\n');
  process.exit(0);
}

const MAXIMO = 30;
const colunas = (...c) => c[0].padEnd(2) + c[1].padEnd(18) + c[2].padEnd(20) + c[3].padEnd(24) + c[4].padEnd(34) + c[5];
console.log(colunas('', 'QUANDO', 'UTILIZADOR', 'TIPO', 'ORIGEM', 'MOTIVO'));
console.log('─'.repeat(120));
for (const l of logins.slice(0, MAXIMO)) {
  const origem = descreverOrigem(l.ip ?? 'este PC');
  console.log(colunas(novos.has(l.id) ? '*' : '', quando(l.quando), (l.utilizador ?? '—').slice(0, 19), l.tipoTexto.slice(0, 23), origem.slice(0, 33), l.motivo));
}
if (logins.length > MAXIMO) console.log(`  … e mais ${logins.length - MAXIMO} mais antigos.`);
if (novos.size) console.log('\n* = novo desde a última leitura');

const lista = alertas(logins);
const descrever = {
  rajada: (a) => {
    const minutos = Math.max(1, Math.round((Date.parse(a.ate) - Date.parse(a.de)) / 60_000));
    return `! RAJADA        ${a.n} tentativas em ${minutos} min, a partir de ${descreverOrigem(a.origem)} (${quando(a.de)})`;
  },
  'pela-rede': (a) => `! PELA REDE     ${a.n} tentativas vindas de ${descreverOrigem(a.origem)}, com: ${a.utilizadores.join(', ')}`,
  'utilizador-inexistente': (a) => `· NOMES FALSOS  ${a.n} tentativas com utilizadores que não existem neste PC: ${a.utilizadores.join(', ')}`,
};

if (lista.length) {
  console.log(`\nAlertas (rajada = ${RAJADA.tentativas} ou mais tentativas em ${RAJADA.minutos} minutos):`);
  for (const a of lista) console.log('  ' + descrever[a.tipo](a));
} else {
  console.log('\nSem alertas: nenhuma rajada, nada vindo de outros aparelhos da rede.');
}
console.log();
