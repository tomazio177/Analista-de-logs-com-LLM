import { interfaceLocal, enderecosDaRede } from './rede.js';
import { descobrirAparelhos } from './descobrir.js';
import { PORTAS, portasAbertas } from './portas.js';
import { emParalelo } from './util.js';
import { abrir, guardar, guardarTriagem, macsConhecidos, ultimoVarrimento } from './historico.js';
import { compararVarrimentos } from './comparar.js';
import { lerLoginsFalhados, TIPOS_LOGIN } from './eventos.js';
import { prepararEntrada, triar, combinar, explicarErro } from './triagem.js';

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
const mudancas = anterior ? compararVarrimentos(anterior, aparelhos, macsConhecidos(db)) : [];
const varrimentoId = guardar(db, cidr, aparelhos);

if (!anterior) {
  console.log('\nPrimeiro varrimento desta rede: fica guardado como linha de base.');
  console.log('Corre outra vez mais tarde para ver o que mudou.\n');
} else {
  mostrarMudancas(mudancas, anterior.data);
}

// Os logins falhados olham para a mesma janela de tempo que a comparação: desde o último varrimento.
const desde = anterior ? new Date(anterior.data) : new Date(Date.now() - 24 * 60 * 60 * 1000);
const logins = await lerLoginsFalhados(desde);
mostrarLogins(logins, desde);

// Primeiro os factos, sempre; a opinião do modelo vem depois e só acrescenta.
// Se a triagem falhar, nada do que foi detetado fica por mostrar.
if (mudancas.length || logins.grupos.length) {
  if (!process.env.ANTHROPIC_API_KEY) {
    console.log('Triagem com LLM desligada: falta a ANTHROPIC_API_KEY (ver README).\n');
  } else {
    const entrada = prepararEntrada({ rede: cidr, local, aparelhos, mudancas, grupos: logins.grupos, desde });
    const n = entrada.sinais.length;
    console.log(`A pedir ao modelo a triagem de ${n} ${n === 1 ? 'sinal' : 'sinais'}…`);
    try {
      const { triagem, modelo } = await triar(entrada);
      guardarTriagem(db, varrimentoId, modelo, entrada, triagem);
      mostrarTriagem(combinar(entrada, triagem), modelo);
    } catch (erro) {
      console.log(`A triagem falhou: ${explicarErro(erro)}\n`);
    }
  }
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
    novo: (m) => `+ NOVO          ${m.aparelho.ip}  ${m.aparelho.mac}  nunca visto antes`,
    voltou: (m) => `↺ VOLTOU        ${m.aparelho.ip}  ${m.aparelho.mac}`,
    desapareceu: (m) => `− SAIU          ${m.aparelho.ip}  ${m.aparelho.mac}`,
    'mudou-ip': (m) => `~ IP MUDOU      ${m.aparelho.mac}  ${m.de} → ${m.aparelho.ip}`,
    'portas-abertas': (m) => `! PORTA ABRIU   ${m.aparelho.ip}  ${m.portas.map((p) => `${p} ${PORTAS[p]}`).join(', ')}`,
    'portas-fechadas': (m) => `· PORTA FECHOU  ${m.aparelho.ip}  ${m.portas.map((p) => `${p} ${PORTAS[p]}`).join(', ')}`,
  };

  console.log(`\nMudanças desde o último varrimento (${quando}):`);
  for (const m of lista) console.log(`  ${m.id.padEnd(4)}${descrever[m.tipo](m)}`);
  console.log();
}

function mostrarLogins(logins, desde) {
  if (logins.estado === 'indisponivel') return; // não é Windows
  if (logins.estado === 'sem-permissao') {
    console.log('Logins falhados: sem acesso ao registo de Segurança do Windows.');
    console.log('Corre como administrador, ou junta o teu utilizador ao grupo "Event Log Readers" (ver README).\n');
    return;
  }
  if (logins.estado === 'erro') {
    console.log(`Logins falhados: não consegui ler o registo de Segurança (${logins.motivo}).\n`);
    return;
  }

  const quando = desde.toLocaleString('pt-PT');
  if (!logins.grupos.length) {
    console.log(`Nenhum login falhado neste PC desde ${quando}.\n`);
    return;
  }

  const aviso = logins.truncado ? ` — só li os ${logins.total} mais recentes` : '';
  console.log(`Logins falhados neste PC desde ${quando}${aviso}:`);
  for (const g of logins.grupos) {
    const outros = g.utilizadoresDistintos - g.utilizadores.length;
    const quem = g.utilizadores.map((u) => u.valor).join(', ') + (outros > 0 ? ` e mais ${outros}` : '');
    const tipo = TIPOS_LOGIN[g.tipoLogin] ?? `tipo ${g.tipoLogin}`;
    console.log(`  ${g.id.padEnd(4)}${String(g.tentativas).padStart(5)}×  ${g.origem.padEnd(16)}${tipo.padEnd(20)}${g.motivos[0].valor.padEnd(24)}${quem}`);
  }
  console.log();
}

function mostrarTriagem({ resumo, itens, avisos }, modelo) {
  const rotulo = { alta: 'ALTA', media: 'MÉDIA', baixa: 'BAIXA', info: 'INFO' };
  console.log(`\nTriagem (${modelo}):\n  ${resumo}\n`);
  for (const i of itens) {
    const regra = i.minima ? `  [subida pelas regras: mínimo ${rotulo[i.minima].toLowerCase()}]` : '';
    console.log(`  ${rotulo[i.gravidade].padEnd(7)}${i.id.padEnd(4)}${i.explicacao}${regra}`);
    if (i.acao) console.log(`             → ${i.acao}`);
  }
  for (const a of avisos) console.log(`  Aviso: ${a}`);
  console.log();
}
