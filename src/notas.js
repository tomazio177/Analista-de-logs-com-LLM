import readline from 'node:readline/promises';
import { abrir, ultimaTriagemValida, guardarNota, listarNotas, apagarNota } from './historico.js';
import { alvoDaPergunta } from './triagem.js';

// npm run responder           responde às perguntas da última triagem
// npm run notas               mostra as respostas guardadas
// npm run notas -- --apagar 3 apaga a resposta n.º 3
const db = abrir();
const args = process.argv.slice(2);
const quando = (iso) => new Date(iso).toLocaleString('pt-PT', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' });

function descreverAlvo(n) {
  if (n.alvo_tipo === 'aparelho') return `aparelho ${n.alvo}`;
  if (n.alvo_tipo === 'alerta') {
    const [tipo, origem, de] = n.alvo.split('|');
    return `alerta ${tipo} de ${origem}, a ${quando(de)}`;
  }
  return 'geral';
}

if (args.includes('--apagar')) {
  const id = Number(args[args.indexOf('--apagar') + 1]);
  console.log(apagarNota(db, id) ? `\nResposta ${id} apagada.\n` : `\nNão há nenhuma resposta com o n.º ${args[args.indexOf('--apagar') + 1]}.\n`);
} else if (args.includes('--listar')) {
  const notas = listarNotas(db);
  if (!notas.length) console.log('\nAinda não há respostas guardadas. Depois de uma triagem: npm run responder\n');
  for (const n of notas) {
    console.log(`\n${n.id}. [${descreverAlvo(n)}] ${quando(n.data)}`);
    if (n.pergunta) console.log(`   P: ${n.pergunta}`);
    console.log(`   R: ${n.texto}`);
  }
  if (notas.length) console.log('\nPara apagar uma: npm run notas -- --apagar <n.º>\n');
} else {
  await responder();
}

async function responder() {
  const triagem = ultimaTriagemValida(db);
  const perguntas = triagem ? (JSON.parse(triagem.resposta).perguntas_ao_dono ?? []) : [];
  if (!perguntas.length) {
    console.log('\nA última triagem não deixou perguntas. Corre primeiro: npm run triagem\n');
    return;
  }
  const ancoras = triagem.ancoras && JSON.parse(triagem.ancoras);
  console.log(`\nPerguntas da triagem de ${quando(triagem.data)}. Escreve a resposta e Enter; Enter sem nada passa à seguinte.\n`);

  // Linha a linha pelo iterador, e não com rl.question: este guarda as linhas que chegam antes da pergunta
  // (respostas coladas de uma vez) e acaba bem quando a entrada fecha.
  const rl = readline.createInterface({ input: process.stdin });
  const linhas = rl[Symbol.asyncIterator]();
  let guardadas = 0;
  for (const [i, p] of perguntas.entries()) {
    const alvo = alvoDaPergunta(p, ancoras);
    process.stdout.write(`${i + 1}. ${alvo.pergunta}\n> `);
    const { value, done } = await linhas.next();
    if (done) break;
    const texto = value.trim();
    if (!texto) continue;
    guardarNota(db, { ...alvo, texto });
    guardadas++;
  }
  rl.close();
  console.log(`\n${guardadas} resposta(s) guardada(s). A próxima triagem já as recebe. Para as ver: npm run notas\n`);
}
