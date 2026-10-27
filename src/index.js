import { abrir } from './historico.js';
import { varrer } from './motor.js';
import { lerOpcoes, emitir, progresso, mostrarVarrimento } from './mostrar.js';

// npm run scan [-- --aprovar] [--json]
const { values } = lerOpcoes({ options: { aprovar: { type: 'boolean' }, json: { type: 'boolean' } } });
const resultado = await varrer(abrir(), { aprovar: values.aprovar, aoProgredir: progresso(values.json) });
emitir(resultado, { json: values.json, mostrar: mostrarVarrimento });
