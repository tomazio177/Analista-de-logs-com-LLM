import readline from 'node:readline/promises';
import { abrir } from './historico.js';
import { perguntas, responder, respostas, apagarResposta } from './motor.js';
import { lerOpcoes, emitir, mostrarFalha, mostrarPerguntas, mostrarRespostas } from './mostrar.js';

// npm run responder                              responde às perguntas da última triagem, uma a uma
// npm run responder -- --perguntas               só mostra as perguntas
// npm run responder -- --pergunta 2 --texto "…"  responde à pergunta 2 sem conversa (é o que a app usa)
// npm run notas                                  mostra as respostas guardadas
// npm run notas -- --apagar 3                    apaga a resposta n.º 3
// Todos aceitam --json.
const { values } = lerOpcoes({
  options: {
    listar: { type: 'boolean' }, apagar: { type: 'string' }, perguntas: { type: 'boolean' },
    pergunta: { type: 'string' }, texto: { type: 'string' }, json: { type: 'boolean' },
  },
});
const db = abrir();
const json = values.json;

if (values.apagar !== undefined) {
  emitir(apagarResposta(db, Number(values.apagar)), { json, mostrar: () => console.log(`\nResposta ${values.apagar} apagada.\n`) });
} else if (values.listar) {
  emitir(respostas(db), { json, mostrar: mostrarRespostas });
} else if (values.pergunta !== undefined) {
  emitir(responder(db, { n: Number(values.pergunta), texto: values.texto }), { json, mostrar: () => console.log('\nResposta guardada.\n') });
} else if (values.perguntas || json) {
  emitir(perguntas(db), { json, mostrar: mostrarPerguntas });
} else {
  await conversa();
}

async function conversa() {
  const r = perguntas(db);
  if (!r.ok) return emitir(r, { json: false });
  const porResponder = r.perguntas.filter((p) => !p.respondida);
  if (!porResponder.length) {
    console.log('\nA última triagem não deixou perguntas por responder.\n');
    return;
  }
  console.log('\nEscreve a resposta e Enter; Enter sem nada passa à seguinte.\n');

  // Linha a linha pelo iterador, e não com rl.question: este guarda as linhas que chegam antes da pergunta
  // (respostas coladas de uma vez) e acaba bem quando a entrada fecha.
  const rl = readline.createInterface({ input: process.stdin });
  const linhas = rl[Symbol.asyncIterator]();
  let guardadas = 0;
  for (const p of porResponder) {
    process.stdout.write(`${p.n}. ${p.pergunta}\n> `);
    const { value, done } = await linhas.next();
    if (done) break;
    if (!value.trim()) continue;
    const g = responder(db, { n: p.n, texto: value });
    if (g.ok) guardadas++;
    else mostrarFalha(g);
  }
  rl.close();
  console.log(`\n${guardadas} resposta(s) guardada(s). A próxima triagem já as recebe. Para as ver: npm run notas\n`);
}
