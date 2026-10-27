import { abrir } from './historico.js';
import { fazerTriagem, dadosDaTriagem } from './motor.js';
import { lerOpcoes, emitir, mostrarTriagem } from './mostrar.js';

// npm run triagem [-- --ver-dados] [--json]
const { values } = lerOpcoes({ options: { 'ver-dados': { type: 'boolean' }, json: { type: 'boolean' } } });
const db = abrir();

// Mostra exatamente o que seria enviado, sem gastar nada.
if (values['ver-dados']) {
  const r = dadosDaTriagem(db);
  if (r.ok && !values.json) console.log(JSON.stringify(r.dados, null, 2));
  else emitir(r, { json: values.json });
} else {
  if (!values.json) console.log('\nA pedir a triagem…');
  emitir(await fazerTriagem(db), { json: values.json, mostrar: mostrarTriagem });
}
