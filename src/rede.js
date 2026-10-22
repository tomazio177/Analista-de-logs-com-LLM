import os from 'node:os';

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
