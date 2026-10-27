import { test } from 'node:test';
import assert from 'node:assert/strict';
import { comBaseTemporaria } from './ajuda.js';
import { abrir, guardar, guardarLogins, guardarLeitura } from '../src/historico.js';
import { recolher, triar, ErroTriagem, MODELO, DIAS } from '../src/triagem.js';

const FUSO = Intl.DateTimeFormat().resolvedOptions().timeZone;
const local = (iso) => new Date(iso).toLocaleString('sv-SE', { timeZone: FUSO });
const AGORA = new Date('2026-10-27T12:00:00Z');

// Uma casa com router, o PC e um telemóvel com MAC privado, e a rajada do Edge de 22/10.
function casaDeTeste(db) {
  guardar(db, { cidr: '192.168.1.0/24', router: '00:00:5e:00:53:01' }, [
    { mac: '00:00:5e:00:53:01', ip: '192.168.1.1', portas: [53, 80], nome: 'router.lan', proprio: false },
    { mac: '00:00:5e:00:53:4d', ip: '192.168.1.77', portas: [139, 445], nome: 'PC-Sala.lan', proprio: true },
    { mac: '02:00:00:00:00:45', ip: '192.168.1.69', portas: [], nome: 'Telemovel-Ana.lan', proprio: false },
  ], '2026-10-27T11:00:00Z');
  guardarLogins(db, Array.from({ length: 28 }, (_, i) => ({
    id: 100 + i, quando: new Date(Date.parse('2026-10-22T14:45:00Z') + i * 1100).toISOString(),
    utilizador: null, tipo: 2, codigo: '0xc000006a', ip: null, estacao: null, processo: 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  })));
  guardarLeitura(db, '2026-10-27T10:00:00Z', '2026-10-22T14:55:28Z');
}

test('sem varrimentos não há nada para enviar', () =>
  comBaseTemporaria((ficheiro) => assert.equal(recolher(abrir(ficheiro), AGORA), null)));

test('os dados enviados marcam este PC pelo varrimento e escondem o MAC completo', () =>
  comBaseTemporaria((ficheiro) => {
    const db = abrir(ficheiro);
    casaDeTeste(db);
    const { varrimentoId, dados } = recolher(db, AGORA);

    assert.equal(varrimentoId, 1);
    assert.deepEqual(dados.aparelhos.map((a) => [a.ip, a.este_pc]), [['192.168.1.1', false], ['192.168.1.69', false], ['192.168.1.77', true]]);
    assert.equal(dados.aparelhos[0].fabricante_mac, '00:00:5e');
    assert.equal(dados.aparelhos[1].fabricante_mac, null); // MAC privado: o prefixo não diz o fabricante
    assert.equal(JSON.stringify(dados).includes('00:53:4d'), false);
  }));

test('os logins seguem agrupados, com a leitura do ritmo e os períodos que nenhuma leitura cobre', () =>
  comBaseTemporaria((ficheiro) => {
    const db = abrir(ficheiro);
    casaDeTeste(db);
    const { logins_falhados: l } = recolher(db, AGORA).dados;

    assert.equal(l.total, 28);
    assert.equal(l.agrupados.length, 1);
    assert.equal(l.agrupados[0].tentativas, 28);
    assert.equal(l.agrupados[0].processo, 'msedge.exe');
    assert.equal(l.alertas_das_regras[0].ritmoDeMaquina, true);
    assert.equal(l.ultima_leitura, local('2026-10-27T10:00:00Z'));
    const inicio = new Date(AGORA.getTime() - DIAS * 86_400_000).toISOString();
    assert.deepEqual(l.periodos_sem_registo, [{ de: local(inicio), ate: local('2026-10-22T14:55:28Z') }]);
  }));

// Um cliente falso: devolve uma resposta fixa e guarda o pedido para se poder inspecionar.
function clienteFalso(resposta) {
  const cliente = {
    beta: { messages: { create: async (pedido) => ((cliente.pedido = pedido), { model: MODELO, stop_reason: 'end_turn', usage: {}, ...resposta }) } },
  };
  return cliente;
}
const texto = (obj) => ({ content: [{ type: 'text', text: typeof obj === 'string' ? obj : JSON.stringify(obj) }] });

const achado = {
  titulo: 'Rajada de palavras-passe erradas vinda do Edge',
  gravidade: 'baixa',
  nos_dados: ['28 tentativas do msedge.exe', 'ritmoDeMaquina: true'],
  conhecimento_geral: ['O Edge pode repetir uma credencial guardada.'],
  conclusao: 'Provavelmente o Edge a repetir uma credencial antiga.',
  acao_sugerida: 'Rever as palavras-passe guardadas no Edge.',
  confianca: 'media',
};
const valida = { nivel_geral: 'tranquilo', resumo: 'Nada de grave.', achados: [achado], perguntas_ao_dono: [] };

test('triar envia o esquema com os campos separados e devolve a resposta validada', async () => {
  const cliente = clienteFalso(texto(valida));
  const { triagem, modelo, bruto } = await triar({ aparelhos: [] }, cliente);

  assert.deepEqual(triagem, valida);
  assert.equal(modelo, MODELO);
  assert.equal(bruto, JSON.stringify(valida)); // o texto tal e qual, para ficar guardado
  const { format } = cliente.pedido.output_config;
  assert.equal(format.type, 'json_schema');
  assert.deepEqual(Object.keys(format.schema.properties.achados.items.properties),
    ['titulo', 'gravidade', 'nos_dados', 'conhecimento_geral', 'conclusao', 'acao_sugerida', 'confianca']);
  assert.equal(cliente.pedido.fallbacks, 'default');
});

test('um achado sem factos dos dados é rejeitado, e o texto do modelo segue com o erro', async () => {
  const semFactos = { ...valida, achados: [{ ...achado, nos_dados: [] }] };
  await assert.rejects(
    triar({}, clienteFalso(texto(semFactos))),
    (e) => e instanceof ErroTriagem && /nos_dados/.test(e.message) && e.bruto === JSON.stringify(semFactos),
  );
  // Campos antigos, de antes da separação, também não passam.
  const antigo = { ...valida, achados: [{ ...achado, evidencia: 'x' }] };
  await assert.rejects(triar({}, clienteFalso(texto(antigo))), ErroTriagem);
});

test('recusas, respostas cortadas e texto que não é JSON dão erros próprios', async () => {
  await assert.rejects(triar({}, clienteFalso({ stop_reason: 'refusal', content: [] })), /recusou/);
  await assert.rejects(triar({}, clienteFalso({ stop_reason: 'max_tokens', ...texto('{"resumo":') })), /cortada/);
  await assert.rejects(triar({}, clienteFalso({ content: [] })), /não trouxe texto/);
  await assert.rejects(triar({}, clienteFalso(texto('{"resumo":'))), /não é JSON válido/);
});
