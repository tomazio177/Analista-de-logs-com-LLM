import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import dns from 'node:dns/promises';
import { emParalelo, comTimeout, limpar } from './util.js';

const correr = promisify(execFile);
const WINDOWS = process.platform === 'win32';

// Procura "TTL=" na resposta em vez de confiar no código de saída:
// no Windows, "Host de destino inacessível" também sai com código 0.
async function ping(ip) {
  const args = WINDOWS ? ['-n', '1', '-w', '700', ip] : ['-c', '1', '-W', '1', ip];
  try {
    const { stdout } = await correr('ping', args, { windowsHide: true });
    return /ttl=/i.test(stdout);
  } catch {
    return false;
  }
}

// Lê a tabela ARP do sistema: a correspondência IP → MAC que o PC aprendeu.
// Funciona com o formato do Windows ("192.168.0.1  60-e3-27-...") e do Linux/macOS ("(192.168.0.1) at 60:e3:27:...").
export async function tabelaArp() {
  const { stdout } = await correr('arp', ['-a'], { windowsHide: true });
  const tabela = new Map();
  const linha = /(\d{1,3}(?:\.\d{1,3}){3})\)?\s+(?:at\s+)?([0-9a-f]{1,2}(?:[-:][0-9a-f]{1,2}){5})/gi;

  for (const [, ip, macBruto] of stdout.matchAll(linha)) {
    const mac = macBruto.toLowerCase().replaceAll('-', ':');
    if (mac === 'ff:ff:ff:ff:ff:ff' || mac.startsWith('01:00:5e')) continue; // broadcast e multicast
    tabela.set(ip, mac);
  }
  return tabela;
}

// Em casa, o nome vem quase sempre do router, que o copia do pedido DHCP — ou seja, quem o escolhe é o próprio aparelho.
async function nomeDe(ip) {
  const nomes = await comTimeout(dns.reverse(ip), 1500, []);
  return limpar(nomes[0], 64);
}

// Um aparelho conta como presente se respondeu ao ping OU se apareceu na tabela ARP.
// Muitos telemóveis e PCs com firewall ignoram o ping, mas para o ping sequer sair
// o PC teve de perguntar "quem tem este IP?" — e a resposta fica na tabela ARP.
export async function descobrirAparelhos(local, hosts, aoProgredir = () => {}) {
  let feitos = 0;
  const responderam = await emParalelo(hosts, 48, async (ip) => {
    const vivo = await ping(ip);
    aoProgredir(++feitos, hosts.length);
    return vivo ? ip : null;
  });

  const arp = await tabelaArp();
  const naRede = new Set(hosts);
  const ips = new Set(responderam.filter(Boolean));
  for (const ip of arp.keys()) if (naRede.has(ip)) ips.add(ip);

  const aparelhos = [...ips].map((ip) => ({
    ip,
    mac: ip === local.ip ? local.mac : arp.get(ip) ?? `sem-mac:${ip}`,
    respondeuPing: responderam.includes(ip),
    proprio: ip === local.ip,
  }));

  await emParalelo(aparelhos, 16, async (a) => {
    a.nome = await nomeDe(a.ip);
  });

  return aparelhos.sort((a, b) => Number(a.ip.split('.')[3]) - Number(b.ip.split('.')[3]));
}
