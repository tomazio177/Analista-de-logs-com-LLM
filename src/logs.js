import { abrir } from './historico.js';
import { lerLogs } from './motor.js';
import { lerOpcoes, emitir, mostrarLogs } from './mostrar.js';

// npm run logs [-- 30] [--json]   (dias: de 1 a 365; por omissão, 7)
const { values, positionals } = lerOpcoes({ options: { json: { type: 'boolean' } }, allowPositionals: true });
const resultado = await lerLogs(abrir(), { dias: Number(positionals[0] ?? 7) });
emitir(resultado, { json: values.json, mostrar: mostrarLogs });
