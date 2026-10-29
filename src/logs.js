import { abrir } from './historico.js';
import { lerLogs } from './motor.js';
import { lerOpcoes, emitir, mostrarLogs } from './mostrar.js';

// npm run logs [-- 30] [--json]   (dias: de 1 a 365; por omissão, 7)
// --dados=<pasta>: a app corre este comando como administrador, e o Windows não passa as variáveis de ambiente
// a um processo elevado. Assim os logins vão para a mesma pasta de dados que a app usa.
const { values, positionals } = lerOpcoes({ options: { json: { type: 'boolean' }, dados: { type: 'string' } }, allowPositionals: true });
if (values.dados) process.env.ANALISTA_DADOS = values.dados;
const resultado = await lerLogs(abrir(), { dias: Number(positionals[0] ?? 7) });
emitir(resultado, { json: values.json, mostrar: mostrarLogs });
