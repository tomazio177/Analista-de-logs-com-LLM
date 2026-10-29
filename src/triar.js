import { abrir } from './historico.js';
import { fazerTriagem, dadosDaTriagem, ultimaTriagem } from './motor.js';
import { lerOpcoes, emitir, mostrarTriagem } from './mostrar.js';

// npm run triagem [-- --ver-dados | --ultima] [--json]   (--ultima mostra a última guardada, sem gastar a API)
const { values } = lerOpcoes({ options: { 'ver-dados': { type: 'boolean' }, ultima: { type: 'boolean' }, json: { type: 'boolean' } } });
const db = abrir();

// Mostra exatamente o que seria enviado, sem gastar nada.
if (values['ver-dados']) {
  const r = dadosDaTriagem(db);
  if (r.ok && !values.json) console.log(JSON.stringify(r.dados, null, 2));
  else emitir(r, { json: values.json });
} else if (values.ultima) {
  emitir(ultimaTriagem(db), { json: values.json, mostrar: mostrarTriagem });
} else {
  if (!values.json) console.log('\nA pedir a triagem…');
  emitir(await fazerTriagem(db), { json: values.json, mostrar: mostrarTriagem });
}
