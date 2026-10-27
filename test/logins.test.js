import { test } from 'node:test';
import assert from 'node:assert/strict';
import { interpretar, alertas, RAJADA, RITMO_DE_MAQUINA } from '../src/logins.js';

const evento = (campos, quando = '2026-10-22T14:45:00.000Z', id = 1) => ({ id, quando, campos: { LogonType: '2', SubStatus: '0xc000006a', ...campos } });

test('lê o motivo do SubStatus e, quando vem a zero, do Status', () => {
  assert.equal(interpretar(evento({ SubStatus: '0xC0000064' })).motivo, 'utilizador não existe');
  assert.equal(interpretar(evento({ SubStatus: '0x0', Status: '0xc0000234' })).motivo, 'conta bloqueada');
});

test('IPs locais e campos "-" ficam vazios; IPv4 dentro de IPv6 fica só IPv4', () => {
  const local = interpretar(evento({ IpAddress: '127.0.0.1', TargetUserName: '-', ProcessName: '-' }));
  assert.equal(local.ip, null);
  assert.equal(local.utilizador, null);
  assert.equal(local.processo, null);
  assert.equal(interpretar(evento({ IpAddress: '::ffff:192.168.1.50', LogonType: '3' })).ip, '192.168.1.50');
  assert.equal(interpretar(evento({ LogonType: '3' })).pelaRede, true);
});

// Tentativas da mesma origem, separadas por `segundos`.
const serie = (n, segundos, extra = {}) =>
  Array.from({ length: n }, (_, i) => ({
    id: i, quando: new Date(Date.parse('2026-10-22T14:45:00Z') + i * segundos * 1000).toISOString(),
    utilizador: null, tipo: 2, codigo: '0xc000006a', ip: null, processo: 'C:\\Program Files\\Edge\\msedge.exe', pelaRede: false, ...extra,
  }));

test('uma rajada a ritmo de máquina leva a leitura da regra com o alerta', () => {
  const [rajada] = alertas(serie(28, 1.1));
  assert.equal(rajada.tipo, 'rajada');
  assert.equal(rajada.origem, 'este PC');
  assert.equal(rajada.segundosEntreTentativas, 1.1);
  assert.equal(rajada.ritmoDeMaquina, true);
  assert.deepEqual(rajada.processos, ['msedge.exe']);
});

test('uma pessoa a errar a palavra-passe não tem ritmo de máquina; poucas tentativas não são rajada', () => {
  assert.equal(alertas(serie(RAJADA.tentativas, RITMO_DE_MAQUINA * 5))[0].ritmoDeMaquina, false);
  assert.deepEqual(alertas(serie(RAJADA.tentativas - 1, 1)), []);
  // Espalhadas por mais do que a janela da regra também não.
  assert.deepEqual(alertas(serie(RAJADA.tentativas, (RAJADA.minutos * 60) / 2)), []);
});

test('tentativas vindas de outro aparelho e nomes inexistentes têm alertas próprios', () => {
  const vindas = serie(2, 60, { ip: '192.168.1.50', pelaRede: true, utilizador: 'admin', codigo: '0xc0000064' });
  assert.deepEqual(alertas(vindas).map((a) => [a.tipo, a.n]), [['pela-rede', 2], ['utilizador-inexistente', 2]]);
});
