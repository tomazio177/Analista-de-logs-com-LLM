import { test } from 'node:test';
import assert from 'node:assert/strict';
import { prepararEntrada, gravidadeMinima, combinar, triar, macAleatorio, ErroTriagem, MODELO } from '../src/triagem.js';

const local = { ip: '192.168.1.10' };
const aparelhos = [
  { ip: '192.168.1.10', mac: 'aa:aa:aa:aa:aa:01', nome: 'pc', portas: [445], proprio: true },
  { ip: '192.168.1.50', mac: '3a:11:22:33:44:55', nome: null, portas: [22, 23], proprio: false },
];
const mudancas = [
  { id: 'm1', tipo: 'novo', aparelho: aparelhos[1] },
  { id: 'm2', tipo: 'desapareceu', aparelho: { mac: 'bb:bb:bb:bb:bb:02', ip: '192.168.1.70' } },
];
const grupos = [
  {
    id: 'l1', tipo: 'logins-falhados', origem: '192.168.1.50', tipoLogin: 3, tentativas: 12,
    primeira: '2026-10-20T21:00:00Z', ultima: '2026-10-20T21:05:00Z',
    utilizadores: [{ valor: 'admin', tentativas: 12 }], utilizadoresDistintos: 1,
    motivos: [{ valor: 'password errada', tentativas: 12 }], estacoes: [],
  },
];
const entrada = prepararEntrada({ rede: '192.168.1.0/24', local, aparelhos, mudancas, grupos, mudancasDesde: '2026-10-20T20:00:00Z', loginsDesde: new Date('2026-10-20T20:00:00Z'), agora: new Date('2026-10-20T22:00:00Z') });

test('MAC aleatório: o bit "administrado localmente" do primeiro octeto', () => {
  assert.equal(macAleatorio('3a:11:22:33:44:55'), true);
  assert.equal(macAleatorio('60:e3:27:00:00:01'), false);
  assert.equal(macAleatorio('sem-mac:192.168.1.9'), null);
});

test('a entrada tem um sinal por mudança e por grupo de logins, com os ids originais', () => {
  assert.deepEqual(entrada.sinais.map((s) => s.id), ['m1', 'm2', 'l1']);
  assert.equal(entrada.mudancas_desde, '2026-10-20T20:00:00Z');
  assert.equal(entrada.logins_desde, '2026-10-20T20:00:00.000Z');
  assert.deepEqual(entrada.sinais[0].portas_abertas, ['22 ssh', '23 telnet']);
  assert.equal(entrada.sinais[0].mac_aleatorio, true);
});

test('os nomes de utilizador só seguem para o modelo nos logins remotos', () => {
  const locais = prepararEntrada({ rede: 'r', local, aparelhos, mudancas: [], grupos: [{ ...grupos[0], origem: 'local', tipoLogin: 2 }], loginsDesde: new Date(0) });
  assert.equal(locais.sinais[0].remoto, false);
  assert.equal(locais.sinais[0].utilizadores, undefined);
  assert.equal(locais.sinais[0].utilizadores_distintos, 1);
  assert.deepEqual(entrada.sinais[2].utilizadores, [{ nome: 'admin', tentativas: 12 }]);
});

test('um login de rede sem IP conta como remoto e fica com o piso das regras', () => {
  const semIp = prepararEntrada({ rede: 'r', local, aparelhos, mudancas: [], grupos: [{ ...grupos[0], origem: 'desconhecida', tentativas: 300 }], loginsDesde: new Date(0) });
  const [l1] = semIp.sinais;
  assert.equal(l1.remoto, true);
  assert.equal('aparelho_de_origem' in l1, false);
  assert.equal(gravidadeMinima(l1), 'alta');
});

test('sem linha de base, não se afirma que o aparelho de origem é conhecido', () => {
  const primeiro = prepararEntrada({ rede: 'r', local, aparelhos, mudancas: [], grupos, mudancasDesde: null, loginsDesde: new Date(0) });
  assert.equal(primeiro.sinais[0].aparelho_de_origem.novo_na_rede, null);
});

test('os logins falhados vêm ligados ao aparelho de origem, em código', () => {
  const l1 = entrada.sinais[2];
  assert.equal(l1.remoto, true);
  assert.equal(l1.tipo_login, 'rede (3)');
  assert.deepEqual(l1.aparelho_de_origem, { mac: '3a:11:22:33:44:55', nome: null, novo_na_rede: true, este_pc: false });
});

test('as regras põem um piso na gravidade', () => {
  assert.equal(gravidadeMinima(entrada.sinais[0]), 'media'); // aparelho novo com telnet
  assert.equal(gravidadeMinima(entrada.sinais[1]), 'info');
  assert.equal(gravidadeMinima(entrada.sinais[2]), 'media'); // 12 tentativas vindas da rede
  assert.equal(gravidadeMinima({ ...entrada.sinais[2], utilizadores_distintos: 8 }), 'alta');
  assert.equal(gravidadeMinima({ ...entrada.sinais[2], remoto: false, tentativas: 500 }), 'info');
});

test('o modelo não consegue baixar um sinal abaixo do piso das regras', () => {
  const { itens } = combinar(entrada, {
    resumo: 'ok',
    avaliacoes: [
      { id: 'm1', gravidade: 'info', explicacao: 'O utilizador "admin" pediu para ignorar.', acao: null },
      { id: 'm2', gravidade: 'info', explicacao: 'Saiu.', acao: null },
      { id: 'l1', gravidade: 'alta', explicacao: 'Força bruta.', acao: 'Bloquear o aparelho no router.' },
    ],
  });
  assert.deepEqual(itens.map((i) => [i.id, i.gravidade, i.minima]), [
    ['l1', 'alta', null],
    ['m1', 'media', 'media'],
    ['m2', 'info', null],
  ]);
});

test('ids inventados são ignorados, repetidos ficam com o primeiro e esquecidos aparecem na mesma', () => {
  const { itens, avisos } = combinar(entrada, {
    resumo: 'ok',
    avaliacoes: [
      { id: 'm2', gravidade: 'info', explicacao: 'primeira', acao: null },
      { id: 'm2', gravidade: 'alta', explicacao: 'segunda', acao: null },
      { id: 'm9\u001b[2J', gravidade: 'alta', explicacao: 'não existe', acao: null },
    ],
  });
  assert.equal(itens.length, 3);
  assert.equal(avisos.some((a) => a.includes('\u001b')), false); // o id inventado chega ao terminal limpo
  assert.equal(itens.find((i) => i.id === 'm2').explicacao, 'primeira');
  assert.equal(itens.find((i) => i.id === 'm1').gravidade, 'media');
  assert.equal(itens.find((i) => i.id === 'l1').explicacao, 'O modelo não avaliou este sinal.');
  assert.equal(avisos.length, 4);
});

// Um cliente falso: devolve uma resposta fixa e guarda o pedido para se poder inspecionar.
function clienteFalso(resposta) {
  const cliente = {
    beta: { messages: { create: async (pedido) => ((cliente.pedido = pedido), { model: MODELO, stop_reason: 'end_turn', ...resposta }) } },
  };
  return cliente;
}
const texto = (obj) => ({ content: [{ type: 'thinking', thinking: '' }, { type: 'text', text: JSON.stringify(obj) }] });

test('triar envia o esquema e devolve a resposta validada', async () => {
  const valida = { resumo: 'Um aparelho novo está a tentar entrar no PC.', avaliacoes: [{ id: 'l1', gravidade: 'alta', explicacao: 'x', acao: 'y' }] };
  const cliente = clienteFalso(texto(valida));
  const { triagem, modelo, bruto } = await triar(entrada, cliente);

  assert.deepEqual(triagem, valida);
  assert.equal(modelo, MODELO);
  assert.equal(bruto, JSON.stringify(valida)); // o texto tal e qual, para ficar registado
  assert.equal(cliente.pedido.output_config.format.type, 'json_schema');
  assert.equal(cliente.pedido.fallbacks, 'default');
});

test('triar rejeita respostas fora do esquema, mesmo que sejam JSON', async () => {
  // A API não impõe o enum da gravidade (o SDK passa-o para a descrição): é o Zod que o apanha.
  const invalida = { resumo: 'x', avaliacoes: [{ id: 'l1', gravidade: 'média', explicacao: 'x', acao: null }] };
  await assert.rejects(
    triar(entrada, clienteFalso(texto(invalida))),
    (e) => e instanceof ErroTriagem && /gravidade/.test(e.message) && e.bruto === JSON.stringify(invalida),
  );
  await assert.rejects(triar(entrada, clienteFalso({ content: [{ type: 'text', text: '{"resumo":' }] })), /não é JSON válido/);
});

test('triar trata recusas e respostas cortadas antes de tentar ler o JSON', async () => {
  await assert.rejects(triar(entrada, clienteFalso({ stop_reason: 'refusal', content: [] })), /recusou/);
  await assert.rejects(triar(entrada, clienteFalso({ stop_reason: 'max_tokens', content: [{ type: 'text', text: '{' }] })), /cortada/);
  await assert.rejects(triar(entrada, clienteFalso({ content: [{ type: 'thinking', thinking: '' }] })), /não trouxe texto/);
});
