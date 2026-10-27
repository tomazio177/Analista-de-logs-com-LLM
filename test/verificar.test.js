import { test } from 'node:test';
import assert from 'node:assert/strict';
import { semApoioNosDados } from '../src/verificar.js';

const dados = {
  aparelhos: [{ ip: '192.168.1.64', fabricante_mac: '78:8c:b5', portas_abertas: ['22 ssh', '80 http', '443 https'] }],
  logins_falhados: { agrupados: [{ processo: 'msedge.exe', primeira: '2026-10-22 09:05:00', ultima: '2026-10-22 09:09:05' }] },
};

test('factos que estão nos dados passam, mesmo com a hora escrita sem zero à esquerda', () => {
  assert.deepEqual(semApoioNosDados(['28 tentativas entre 9:05 e 09:09:05', 'portas 22, 80 e 443 abertas em 192.168.1.64', 'prefixo 78:8c:b5'], dados), []);
});

test('IPs, horas e portas que não estão nos dados ficam assinalados', () => {
  assert.deepEqual(semApoioNosDados(['porta 3389 aberta em 192.168.1.50 às 14:45'], dados), ['192.168.1.50', '14:45', 'porta 3389']);
});
