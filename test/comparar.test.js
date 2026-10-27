import { test } from 'node:test';
import assert from 'node:assert/strict';
import { compararVarrimentos, LIMITE_AUSENCIA } from '../src/comparar.js';

const aparelho = (mac, ip, portas = [], nome = null) => ({ mac, ip, portas, nome });
const varrimento = (...lista) => ({ data: 'antes', aparelhos: new Map(lista.map((a) => [a.mac, { ip: a.ip, portas: a.portas }])) });
const conhecidos = (...lista) => new Map(lista.map((a) => [a.mac, a.nome]));

test('deteta aparelhos novos, mudanças de IP e portas que abriram ou fecharam', () => {
  const router = aparelho('aa', '192.168.1.1', [53, 80]);
  const anterior = varrimento(router);
  const atuais = [aparelho('aa', '192.168.1.2', [80, 23]), aparelho('cc', '192.168.1.30')];

  assert.deepEqual(
    compararVarrimentos(anterior, atuais, conhecidos(router)).map((m) => [m.tipo, m.aparelho.mac, m.portas ?? m.de ?? null]),
    [['mudou-ip', 'aa', '192.168.1.1'], ['portas-abertas', 'aa', [23]], ['portas-fechadas', 'aa', [53]], ['novo', 'cc', null]],
  );
});

test('um MAC novo com o nome de um aparelho conhecido fica assinalado como provável troca de MAC', () => {
  const telemovel = aparelho('3a:00:00:00:00:01', '192.168.1.40', [], 'Telemovel-Ana.lan');
  const [m] = compararVarrimentos(varrimento(telemovel), [aparelho('3e:00:00:00:00:02', '192.168.1.41', [], 'Telemovel-Ana.lan')], conhecidos(telemovel));
  assert.equal(m.tipo, 'novo');
  assert.equal(m.mesmoNomeQue, '3a:00:00:00:00:01');
});

test('"saiu" só aparece uma vez, no varrimento em que a ausência chega ao limite', () => {
  const tv = aparelho('bb', '192.168.1.20');
  const faltou = (faltas) => new Map([['bb', { faltas, ip: '192.168.1.20' }]]);
  const tipos = (faltas) => compararVarrimentos(varrimento(), [], conhecidos(tv), faltou(faltas)).map((m) => m.tipo);

  assert.deepEqual(tipos(LIMITE_AUSENCIA - 2), []);
  assert.deepEqual(tipos(LIMITE_AUSENCIA - 1), ['desapareceu']);
  assert.deepEqual(tipos(LIMITE_AUSENCIA), []);
});

test('quem tinha saído e responde outra vez aparece como "voltou"; uma falha isolada não é notícia', () => {
  const tv = aparelho('bb', '192.168.1.20');
  const faltou = (faltas) => new Map([['bb', { faltas, ip: '192.168.1.20' }]]);
  assert.deepEqual(compararVarrimentos(varrimento(), [tv], conhecidos(tv), faltou(LIMITE_AUSENCIA)).map((m) => m.tipo), ['voltou']);
  assert.deepEqual(compararVarrimentos(varrimento(), [tv], conhecidos(tv), faltou(1)), []);
});
