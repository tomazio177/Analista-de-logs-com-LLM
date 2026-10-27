import os from 'node:os';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const correr = promisify(execFile);

// Adaptadores criados por Docker, WSL, VirtualBox e afins não são a rede de casa.
const VIRTUAIS = /vethernet|virtualbox|vmware|wsl|docker|hyper-v|loopback|tailscale|zerotier/i;

export function interfaceLocal() {
  for (const [nome, enderecos] of Object.entries(os.networkInterfaces())) {
    if (VIRTUAIS.test(nome)) continue;
    for (const e of enderecos ?? []) {
      if (e.family !== 'IPv4' || e.internal || e.address.startsWith('169.254.')) continue;
      return { nome, ip: e.address, mascara: e.netmask, mac: e.mac.toLowerCase() };
    }
  }
  throw new Error('Não encontrei nenhuma interface de rede IPv4 ligada.');
}

const paraNumero = (ip) => ip.split('.').reduce((n, octeto) => n * 256 + Number(octeto), 0);
const paraIp = (n) => [24, 16, 8, 0].map((s) => Math.floor(n / 2 ** s) % 256).join('.');

// Devolve a rede em notação CIDR e a lista de endereços a varrer.
// Redes maiores que /24 ficam limitadas ao /24 do próprio PC: 65 mil pings não é varrer, é inundar.
export function enderecosDaRede({ ip, mascara }) {
  let bits = mascara.split('.').reduce((n, o) => n + Number(o).toString(2).split('1').length - 1, 0);
  if (bits < 24) bits = 24;

  const tamanho = 2 ** (32 - bits);
  const rede = Math.floor(paraNumero(ip) / tamanho) * tamanho;

  const hosts = [];
  for (let n = rede + 1; n < rede + tamanho - 1; n++) hosts.push(paraIp(n));

  return { cidr: `${paraIp(rede)}/${bits}`, hosts };
}

// Telemóveis e alguns PCs inventam um MAC por rede (o "MAC privado"), que pode mudar sozinho.
// Esses MACs têm ligado o bit "administrado localmente" (o 2.º bit menos significativo do 1.º octeto):
// na prática, o 2.º carácter é 2, 6, a ou e. Um fabricante nunca grava um MAC destes de fábrica.
export const macPrivado = (mac) => /^[0-9a-f]{2}:/.test(mac) && (parseInt(mac.slice(0, 2), 16) & 0b10) !== 0;

// O router é o "gateway" da rota por omissão. Cada sistema mostra-a à sua maneira; as linhas com números não mudam com a língua.
// Windows: "0.0.0.0  0.0.0.0  192.168.0.1  192.168.0.104  35" (destino, máscara, gateway, interface, métrica).
export function gatewayDoWindows(texto, ipLocal) {
  const rotas = [...texto.matchAll(/^\s*0\.0\.0\.0\s+0\.0\.0\.0\s+(\d{1,3}(?:\.\d{1,3}){3})\s+(\S+)\s+(\d+)\s*$/gm)]
    .filter(([, , interfaceIp]) => interfaceIp === ipLocal)
    .sort((a, b) => Number(a[3]) - Number(b[3]));
  return rotas[0]?.[1] ?? null;
}

// Linux: "default via 192.168.0.1 dev wlan0 proto dhcp metric 600".
export function gatewayDoLinux(texto, nomeInterface) {
  const rotas = [...texto.matchAll(/^default via (\S+) dev (\S+)/gm)];
  return (rotas.find(([, , dev]) => dev === nomeInterface) ?? rotas[0])?.[1] ?? null;
}

// macOS: "gateway: 192.168.0.1".
export const gatewayDoMac = (texto) => texto.match(/gateway:\s*(\d{1,3}(?:\.\d{1,3}){3})/)?.[1] ?? null;

export async function gatewayPadrao(local) {
  try {
    if (process.platform === 'win32') {
      const { stdout } = await correr('route', ['print', '-4', '0.0.0.0'], { windowsHide: true });
      return gatewayDoWindows(stdout, local.ip);
    }
    if (process.platform === 'darwin') return gatewayDoMac((await correr('route', ['-n', 'get', 'default'])).stdout);
    return gatewayDoLinux((await correr('ip', ['-4', 'route', 'show', 'default'])).stdout, local.nome);
  } catch {
    return null;
  }
}
