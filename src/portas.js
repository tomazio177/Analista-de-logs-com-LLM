import net from 'node:net';
import { emParalelo } from './util.js';

// Poucas portas, escolhidas pelo que dizem sobre um aparelho de casa.
// Uma porta 23 (telnet) ou 445 (partilha de ficheiros) aberta num aparelho que não devia tê-la é o tipo de coisa que queremos ver.
export const PORTAS = {
  21: 'ftp',
  22: 'ssh',
  23: 'telnet',
  53: 'dns',
  80: 'http',
  139: 'netbios',
  443: 'https',
  445: 'smb',
  554: 'rtsp',
  1883: 'mqtt',
  3389: 'rdp',
  5000: 'upnp',
  7000: 'airplay',
  8080: 'http-alt',
  8123: 'home-assistant',
  8443: 'https-alt',
  9100: 'impressora',
  62078: 'iphone-sync',
};

// "Connect scan": tenta abrir uma ligação TCP normal. Se o aparelho aceitar, a porta está aberta.
// É o varrimento mais simples e mais lento, mas não precisa de privilégios de administrador.
export function portaAberta(ip, porta, timeout = 600) {
  return new Promise((resolve) => {
    const socket = net.connect({ host: ip, port: porta });
    const terminar = (aberta) => {
      socket.destroy();
      resolve(aberta);
    };
    socket.setTimeout(timeout, () => terminar(false));
    socket.once('connect', () => terminar(true));
    socket.once('error', () => terminar(false));
  });
}

export async function portasAbertas(ip) {
  const lista = Object.keys(PORTAS).map(Number);
  const resultado = await emParalelo(lista, lista.length, (p) => portaAberta(ip, p));
  return lista.filter((_, i) => resultado[i]);
}
