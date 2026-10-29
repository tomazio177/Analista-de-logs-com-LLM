import { test } from 'node:test';
import assert from 'node:assert/strict';
import { comBaseTemporaria } from './ajuda.js';
import { guardar, guardarLogins, guardarLeitura, guardarNota } from '../src/historico.js';
import { recolher, triar, aplicarRegras, alvoDaPergunta, mesmoEpisodio, respostaAnterior, ErroTriagem, MODELO, DIAS } from '../src/triagem.js';

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

test('as notas do dono seguem presas ao aparelho e ao episódio, e uma rajada nova não herda a nota da antiga', () =>
  comBaseTemporaria((abrirBase) => {
    const db = abrirBase();
    casaDeTeste(db);
    guardarNota(db, { alvoTipo: 'aparelho', alvo: '02:00:00:00:00:45', texto: 'É o telemóvel da Ana.' }, '2026-10-27T18:00:00Z');
    guardarNota(db, { alvoTipo: 'alerta', alvo: 'rajada|este PC|2026-10-22T14:45:00.000Z', texto: 'Fui eu.' }, '2026-10-27T18:01:00Z');
    guardarNota(db, { alvoTipo: 'geral', texto: 'Só eu uso este PC.' }, '2026-10-27T18:02:00Z');
    // Uma rajada nova, noutro dia, do mesmo programa.
    guardarLogins(db, tentativas(6, { base: 40, quando: undefined }).map((l, i) => ({ ...l, quando: new Date(Date.parse('2026-10-26T09:00:00Z') + i * 1000).toISOString() })));

    const { dados, ancoras } = recolher(db, AGORA);
    const ana = dados.aparelhos.find((a) => a.ip === '192.168.1.69');
    assert.deepEqual(ana.notas_do_dono, [{ quando: local('2026-10-27T18:00:00Z'), texto: 'É o telemóvel da Ana.' }]);
    assert.equal(dados.aparelhos.find((a) => a.ip === '192.168.1.1').notas_do_dono, undefined);

    const [antiga, nova] = dados.logins_falhados.alertas_das_regras;
    assert.deepEqual(antiga.notas_do_dono.map((n) => n.texto), ['Fui eu.']);
    assert.equal(nova.notas_do_dono, undefined);
    assert.deepEqual(dados.notas_gerais.map((n) => n.texto), ['Só eu uso este PC.']);

    assert.deepEqual(ancoras[ana.id], { tipo: 'aparelho', alvo: '02:00:00:00:00:45' });
    // A chave guarda agora também o fim; a nota antiga, só com o início, continuou a corresponder (acima).
    assert.deepEqual(ancoras[antiga.id], { tipo: 'alerta', alvo: 'rajada|este PC|2026-10-22T14:45:00.000Z|2026-10-22T14:45:29.700Z' });
  }));

test('a resposta a uma pergunta fica presa ao "sobre"; sem âncora conhecida, ou de triagens antigas, fica geral', () => {
  const ancoras = { d2: { tipo: 'aparelho', alvo: '00:00:5e:00:53:dd' } };
  assert.deepEqual(alvoDaPergunta({ pergunta: 'De quem é o portátil?', sobre: 'd2' }, ancoras), { alvoTipo: 'aparelho', alvo: '00:00:5e:00:53:dd', pergunta: 'De quem é o portátil?' });
  assert.deepEqual(alvoDaPergunta({ pergunta: 'x', sobre: 'd9' }, ancoras), { alvoTipo: 'geral', alvo: null, pergunta: 'x' });
  assert.deepEqual(alvoDaPergunta({ pergunta: 'x', sobre: null }, ancoras), { alvoTipo: 'geral', alvo: null, pergunta: 'x' });
  assert.deepEqual(alvoDaPergunta('Quem estava ao PC?', null), { alvoTipo: 'geral', alvo: null, pergunta: 'Quem estava ao PC?' });
});

test('as perguntas ao dono têm de dizer de que falam', async () => {
  await assert.rejects(triar({}, clienteFalso(texto({ ...valida, perguntas_ao_dono: ['De quem é?'] }))), ErroTriagem);
  const { triagem } = await triar({}, clienteFalso(texto({ ...valida, perguntas_ao_dono: [{ pergunta: 'De quem é?', sobre: 'd2' }] })));
  assert.equal(triagem.perguntas_ao_dono[0].sobre, 'd2');
});

test('o mesmo episódio é reconhecido mesmo quando o início muda', () => {
  const rajada = 'rajada|este PC|2026-10-22T14:45:00.000Z|2026-10-22T14:49:05.000Z';
  // O período de 7 dias cortou o início: a rajada passa a começar mais tarde, mas é a mesma.
  assert.equal(mesmoEpisodio(rajada, 'rajada|este PC|2026-10-22T14:47:30.000Z|2026-10-22T14:49:05.000Z'), true);
  // Chegaram mais tentativas do mesmo episódio.
  assert.equal(mesmoEpisodio(rajada, 'rajada|este PC|2026-10-22T14:45:00.000Z|2026-10-22T14:55:00.000Z'), true);
  // Chave antiga, só com o início.
  assert.equal(mesmoEpisodio('rajada|este PC|2026-10-22T14:45:00.000Z', rajada), true);
  // Outro dia, outra origem ou outro tipo: é outra coisa.
  assert.equal(mesmoEpisodio(rajada, 'rajada|este PC|2026-10-26T09:00:00.000Z|2026-10-26T09:00:05.000Z'), false);
  assert.equal(mesmoEpisodio(rajada, 'rajada|192.168.1.66|2026-10-22T14:45:00.000Z|2026-10-22T14:49:05.000Z'), false);
  assert.equal(mesmoEpisodio(rajada, 'pela-rede|este PC|2026-10-22T14:45:00.000Z|2026-10-22T14:49:05.000Z'), false);
});

test('a resposta anterior é a mais recente sobre o mesmo alvo', () => {
  const notas = [
    { alvo_tipo: 'aparelho', alvo: 'aa', pergunta: 'De quem é?', texto: 'Não sei.', data: '1' },
    { alvo_tipo: 'aparelho', alvo: 'aa', pergunta: 'Já sabes?', texto: 'É da Ana.', data: '2' },
    { alvo_tipo: 'geral', alvo: null, pergunta: 'Mais alguém usa este PC?', texto: 'Não.', data: '3' },
  ];
  assert.equal(respostaAnterior(notas, { alvoTipo: 'aparelho', alvo: 'aa', pergunta: 'x' }).texto, 'É da Ana.');
  assert.equal(respostaAnterior(notas, { alvoTipo: 'aparelho', alvo: 'bb', pergunta: 'x' }), null);
  assert.equal(respostaAnterior(notas, { alvoTipo: 'geral', alvo: null, pergunta: '  mais alguém usa este PC? ' }).texto, 'Não.');
  assert.equal(respostaAnterior(notas, { alvoTipo: 'geral', alvo: null, pergunta: 'Outra pergunta?' }), null);
});
