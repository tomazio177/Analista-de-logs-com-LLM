import { test } from 'node:test';
import assert from 'node:assert/strict';
import { compararVarrimentos } from '../src/comparar.js';

const aparelho = (mac, ip, portas = []) => ({ mac, ip, portas });
const varrimento = (...lista) => ({ data: 'antes', aparelhos: new Map(lista.map((a) => [a.mac, { ip: a.ip, portas: a.portas }])) });
const conhecidos = (...lista) => new Map(lista.map((a) => [a.mac, a.portas]));

test('deteta novos, saídas, mudanças de IP e de portas, com ids por ordem', () => {
  const router = aparelho('aa', '192.168.1.1', [80]);
  const tv = aparelho('bb', '192.168.1.20');
  const anterior = varrimento(router, tv);
  const atuais = [aparelho('aa', '192.168.1.2', [80, 23]), aparelho('cc', '192.168.1.30')];

  assert.deepEqual(
    compararVarrimentos(anterior, atuais, conhecidos(router, tv)).map((m) => [m.id, m.tipo, m.aparelho.mac]),
    [['m1', 'mudou-ip', 'aa'], ['m2', 'portas-abertas', 'aa'], ['m3', 'novo', 'cc'], ['m4', 'desapareceu', 'bb']],
  );
});

test('quem voltou compara as portas com a última vez que foi visto', () => {
  // O PC esteve fora no varrimento anterior; da última vez que apareceu não tinha o RDP aberto.
  const router = aparelho('aa', '192.168.1.1');
  const pc = aparelho('dd', '192.168.1.40', [445]);
  const mudancas = compararVarrimentos(varrimento(router), [router, aparelho('dd', '192.168.1.40', [445, 3389])], conhecidos(router, pc));

  assert.deepEqual(mudancas.map((m) => [m.tipo, m.portas]), [['voltou', undefined], ['portas-abertas', [3389]]]);
});
