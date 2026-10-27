import { test } from 'node:test';
import assert from 'node:assert/strict';
import { gatewayDoWindows, gatewayDoLinux, gatewayDoMac, enderecosDaRede, macPrivado } from '../src/rede.js';

// "route print -4 0.0.0.0" num Windows em português, com uma VPN e uma rota persistente.
const ROTAS_WINDOWS = `===========================================================================
Rotas IPv4 Ativas:
Destino de Rede    Máscara de Rede    Gateway        Interface   Métrica
          0.0.0.0          0.0.0.0      192.168.0.1    192.168.0.104     35
          0.0.0.0          0.0.0.0       10.8.0.1       10.8.0.5       5
===========================================================================
Rotas Persistentes:
  Endereço de Rede    Máscara de rede  Endereço de Gateway  Métrica
          0.0.0.0          0.0.0.0      192.168.0.1  Predefinição
`;

test('no Windows, o router é o gateway da rota por omissão da interface usada no varrimento', () => {
  // A VPN tem métrica mais baixa, mas não é a interface da rede de casa.
  assert.equal(gatewayDoWindows(ROTAS_WINDOWS, '192.168.0.104'), '192.168.0.1');
  assert.equal(gatewayDoWindows(ROTAS_WINDOWS, '192.168.1.77'), null);
});

test('no Linux escolhe a rota da interface certa e, sem ela, a primeira', () => {
  const rotas = 'default via 10.0.0.1 dev tun0 proto static\ndefault via 192.168.1.1 dev wlan0 proto dhcp metric 600\n';
  assert.equal(gatewayDoLinux(rotas, 'wlan0'), '192.168.1.1');
  assert.equal(gatewayDoLinux(rotas, 'eth9'), '10.0.0.1');
  assert.equal(gatewayDoLinux('', 'wlan0'), null);
});

test('no macOS lê a linha "gateway:"', () => {
  assert.equal(gatewayDoMac('   route to: default\ndestination: default\n    gateway: 192.168.1.254\n  interface: en0\n'), '192.168.1.254');
  assert.equal(gatewayDoMac('route: writing to routing socket: not in table'), null);
});

test('redes maiores que /24 ficam limitadas ao /24 do PC', () => {
  assert.equal(enderecosDaRede({ ip: '192.168.1.77', mascara: '255.255.255.0' }).hosts.length, 254);
  const grande = enderecosDaRede({ ip: '10.0.5.9', mascara: '255.255.0.0' });
  assert.equal(grande.cidr, '10.0.5.0/24');
  assert.equal(grande.hosts.length, 254);
});

test('MAC privado: o bit "administrado localmente" do primeiro octeto', () => {
  assert.equal(macPrivado('02:00:00:00:00:45'), true);
  assert.equal(macPrivado('06:00:00:00:00:4e'), true);
  assert.equal(macPrivado('00:00:5e:00:53:01'), false);
  assert.equal(macPrivado('sem-mac:192.168.1.9'), false);
});
