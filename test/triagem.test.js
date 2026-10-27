import { test } from 'node:test';
import assert from 'node:assert/strict';
import { comBaseTemporaria } from './ajuda.js';
import { guardar, guardarLogins, guardarLeitura } from '../src/historico.js';
import { recolher, triar, aplicarRegras, ErroTriagem, MODELO, DIAS } from '../src/triagem.js';

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
  comBaseTemporaria((abrirBase) => assert.equal(recolher(abrirBase(), AGORA), null)));

test('os dados enviados marcam este PC pelo varrimento e escondem o MAC completo', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    casaDeTeste(db);
    const { varrimentoId, dados } = recolher(db, AGORA);

    assert.equal(varrimentoId, 1);
    assert.deepEqual(dados.aparelhos.map((a) => [a.ip, a.este_pc]), [['192.168.1.1', false], ['192.168.1.69', false], ['192.168.1.77', true]]);
    assert.equal(dados.aparelhos[0].fabricante_mac, '00:00:5e');
    assert.equal(dados.aparelhos[1].fabricante_mac, null); // MAC privado: o prefixo não diz o fabricante
    assert.equal(JSON.stringify(dados).includes('00:53:4d'), false);
  }));

test('os logins seguem agrupados, com a leitura do ritmo e os períodos que nenhuma leitura cobre', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    casaDeTeste(db);
    const { logins_falhados: l } = recolher(db, AGORA).dados;

    assert.equal(l.total, 28);
    assert.equal(l.agrupados.length, 1);
    assert.equal(l.agrupados[0].tentativas, 28);
    assert.equal(l.agrupados[0].processo, 'msedge.exe');
    assert.equal(l.alertas_das_regras[0].ritmoDeMaquina, true);
    assert.equal(l.ultima_leitura, local('2026-10-27T10:00:00Z'));
    const inicio = new Date(AGORA.getTime() - DIAS * 86_400_000).toISOString();
    assert.deepEqual(l.periodos_possivelmente_incompletos, [{ de: local(inicio), ate: local('2026-10-22T14:55:28Z') }]);
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
  alertas_relacionados: ['a1'],
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
    ['titulo', 'gravidade', 'alertas_relacionados', 'nos_dados', 'conhecimento_geral', 'conclusao', 'acao_sugerida', 'confianca']);
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

// Logins de teste: `pelaRede` com IP é outro aparelho; sem IP e tipo 2 é alguém (ou algum programa) neste PC.
const tentativas = (n, extra) => Array.from({ length: n }, (_, i) => ({
  id: 500 + i + (extra.base ?? 0), quando: new Date(Date.parse('2026-10-26T20:00:00Z') + i * 30_000).toISOString(),
  utilizador: 'x', tipo: 2, codigo: '0xc000006a', ip: null, estacao: null, processo: null, ...extra,
}));

test('os nomes de utilizador dos logins neste PC não saem do PC; os que vêm da rede seguem', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    casaDeTeste(db);
    guardarLogins(db, [
      ...tentativas(2, { utilizador: 'MinhaPass123', codigo: '0xc0000064' }),
      ...tentativas(2, { base: 10, utilizador: 'admin', tipo: 3, ip: '192.168.1.66', codigo: '0xc0000064' }),
    ]);
    const { dados } = recolher(db, AGORA);
    const texto = JSON.stringify(dados);

    assert.equal(texto.includes('MinhaPass123'), false);
    assert.ok(texto.includes('admin'));
    const inexistentes = dados.logins_falhados.alertas_das_regras.filter((a) => a.tipo === 'utilizador-inexistente');
    assert.deepEqual(inexistentes.map((a) => [a.origem, a.utilizadores ?? a.utilizadores_distintos]), [['este PC', 1], ['192.168.1.66', ['admin']]]);
  }));

test('cada alerta segue com id e com a gravidade mínima das regras: só os da rede a têm', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    casaDeTeste(db);
    guardarLogins(db, tentativas(6, { base: 20, tipo: 3, ip: '192.168.1.66' }));
    const alertas = recolher(db, AGORA).dados.logins_falhados.alertas_das_regras;
    assert.deepEqual(alertas.map((a) => [a.id, a.tipo, a.origem, a.gravidade_minima]), [
      ['a1', 'rajada', 'este PC', null],
      ['a2', 'rajada', '192.168.1.66', 'alta'],
      ['a3', 'pela-rede', '192.168.1.66', 'alta'],
    ]);
  }));

const dadosComAlertas = { logins_falhados: { alertas_das_regras: [
  { id: 'a1', tipo: 'rajada', origem: 'este PC', n: 28, gravidade_minima: null },
  { id: 'a2', tipo: 'pela-rede', origem: '192.168.1.66', n: 3, gravidade_minima: 'media' },
] } };

test('meio-termo: o modelo baixa um alerta deste PC, mas não um da rede', () => {
  const resposta = { ...valida, achados: [
    { ...achado, alertas_relacionados: ['a1'], gravidade: 'info' },
    { ...achado, titulo: 'Tentativas do iPhone', alertas_relacionados: ['a2'], gravidade: 'baixa' },
  ] };
  const { triagem, avisos } = aplicarRegras(dadosComAlertas, resposta);
  assert.deepEqual(triagem.achados.map((a) => [a.gravidade, a.gravidade_do_modelo]), [['info', undefined], ['media', 'baixa']]);
  assert.deepEqual(avisos, []);
  assert.equal(triagem.nivel_geral, 'atencao'); // havia um alerta da rede: "tranquilo" não pode ser
});

test('um alerta da rede esquecido pelo modelo e um id inventado dão aviso', () => {
  const { avisos } = aplicarRegras(dadosComAlertas, { ...valida, achados: [{ ...achado, alertas_relacionados: ['a1', 'a9'] }] });
  assert.equal(avisos.length, 2);
  assert.match(avisos[0], /"a9"/);
  assert.match(avisos[1], /a2 .*pelo menos media/);
});
