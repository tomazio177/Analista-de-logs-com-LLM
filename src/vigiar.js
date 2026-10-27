import { abrir } from './historico.js';
import { vigiar } from './motor.js';
import { lerOpcoes, emitir, progresso, mostrarVigia } from './mostrar.js';

// npm run vigiar [-- --aprovar] [--json]: varre a rede, lê os logins falhados e pede a triagem, de seguida.
const { values } = lerOpcoes({ options: { aprovar: { type: 'boolean' }, json: { type: 'boolean' } } });
const resultado = await vigiar(abrir(), { aprovar: values.aprovar, aoProgredir: progresso(values.json) });
emitir(resultado, { json: values.json, mostrar: mostrarVigia });
