import { interfaceLocal, enderecosDaRede, macPrivado } from './rede.js';
import { descobrirAparelhos } from './descobrir.js';
import { PORTAS, portasAbertas } from './portas.js';
import { emParalelo } from './util.js';
import { abrir, guardar, macsConhecidos, ultimoVarrimento, ausenciasPorMac } from './historico.js';
import { compararVarrimentos } from './comparar.js';

const local = interfaceLocal();
const { cidr, hosts } = enderecosDaRede(local);

console.log(`\nAnalista de logs com LLM — rede ${cidr} (interface "${local.nome}", este PC é ${local.ip})`);

const aparelhos = await descobrirAparelhos(local, hosts, (feitos, total) => {
  process.stdout.write(`\rA procurar aparelhos… ${feitos}/${total}`);
});
process.stdout.write('\n');

console.log(`Encontrados ${aparelhos.length} aparelhos. A testar ${Object.keys(PORTAS).length} portas em cada…\n`);
await emParalelo(aparelhos, 8, async (a) => {
  a.portas = await portasAbertas(a.ip);
});

mostrarInventario(aparelhos);

const db = abrir();
const anterior = ultimoVarrimento(db, cidr);
const conhecidos = macsConhecidos(db);
const mudancas = compararVarrimentos(anterior, aparelhos, conhecidos, ausenciasPorMac(db, cidr));
guardar(db, cidr, aparelhos);

if (!anterior) {
  console.log('\nPrimeiro varrimento desta rede: fica guardado como linha de base.');
  console.log('Corre outra vez mais tarde para ver o que mudou.\n');
} else {
  mostrarMudancas(mudancas, anterior.data);
}

function mostrarInventario(lista) {
  const colunas = (...c) => c[0].padEnd(16) + c[1].padEnd(20) + c[2].padEnd(28) + c[3];
  console.log(colunas('IP', 'MAC', 'Nome', 'Portas abertas'));
  console.log('─'.repeat(90));
  for (const a of lista) {
    const nome = (a.proprio ? '(este PC) ' : '') + (a.nome ?? '—');
    const portas = a.portas.map((p) => `${p} ${PORTAS[p]}`).join(', ') || '—';
    console.log(colunas(a.ip, a.mac, nome.slice(0, 27), portas));
  }
}

function mostrarMudancas(lista, desde) {
  const quando = new Date(desde).toLocaleString('pt-PT');
  if (!lista.length) {
    console.log(`\nNada mudou desde o último varrimento (${quando}).\n`);
    return;
  }

  const descrever = {
    novo: (m) =>
      `+ NOVO          ${m.aparelho.ip}  ${m.aparelho.mac}  ` +
      (macPrivado(m.aparelho.mac) ? 'MAC privado: pode ser um telemóvel já conhecido que mudou de MAC' : 'nunca visto antes'),
    voltou: (m) => `↺ VOLTOU        ${m.aparelho.ip}  ${m.aparelho.mac}  (tinha desaparecido)`,
    desapareceu: (m) => `− SAIU          ${m.aparelho.ip}  ${m.aparelho.mac}`,
    'mudou-ip': (m) => `~ IP MUDOU      ${m.aparelho.mac}  ${m.de} → ${m.aparelho.ip}`,
    'portas-abertas': (m) => `! PORTA ABRIU   ${m.aparelho.ip}  ${m.portas.map((p) => `${p} ${PORTAS[p]}`).join(', ')}`,
    'portas-fechadas': (m) => `· PORTA FECHOU  ${m.aparelho.ip}  ${m.portas.map((p) => `${p} ${PORTAS[p]}`).join(', ')}`,
  };

  console.log(`\nMudanças desde o último varrimento (${quando}):`);
  for (const m of lista) console.log('  ' + descrever[m.tipo](m));
  console.log();
}
