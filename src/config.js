import { text } from 'node:stream/consumers';
import { lerOpcoes, emitir } from './mostrar.js';
import { configuracao, guardarChave } from './motor.js';

// npm run chave                         pede a chave da API e guarda-a na pasta de dados
// node src/config.js --json             diz onde estão os dados e se há chave
// node src/config.js --guardar-chave    lê a chave do stdin (é o que a app usa)
// A chave entra pelo stdin e nunca pela linha de comandos, onde outros programas do PC a podiam ver.
const { values } = lerOpcoes({ options: { 'guardar-chave': { type: 'boolean' }, json: { type: 'boolean' } } });

if (values['guardar-chave']) {
  if (!values.json && process.stdin.isTTY) process.stdout.write('\nCola a chave da API (começa por sk-ant-) e carrega Enter:\n> ');
  // Num terminal lê-se uma linha; vinda de outro programa, lê-se tudo até o stdin fechar.
  const chave = process.stdin.isTTY ? await umaLinha() : await text(process.stdin);
  emitir(await guardarChave(chave), {
    json: values.json,
    mostrar: (r) => console.log(`\nChave guardada em ${r.pasta_de_dados}.${r.aviso ? `\n${r.aviso}` : ''}\n`),
  });
} else {
  emitir(configuracao(), {
    json: values.json,
    mostrar: (r) => console.log(`\nDados em: ${r.pasta_de_dados}\nChave da API: ${r.chave_definida ? 'guardada' : 'em falta (npm run chave)'}\n`),
  });
}

function umaLinha() {
  return new Promise((resolve) => {
    process.stdin.setEncoding('utf8');
    process.stdin.once('data', (d) => {
      process.stdin.pause();
      resolve(d.split(/\r?\n/)[0]);
    });
  });
}
