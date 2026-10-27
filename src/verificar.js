// O modelo pode pôr em "nos_dados" algo que não está nos dados. Não dá para verificar uma frase inteira,
// mas dá para verificar o que é fácil de inventar e fácil de procurar: IPs, horas e números de porta.
const IP = /\b\d{1,3}(?:\.\d{1,3}){3}\b/g;
const HORA = /\b\d{1,2}:\d{2}(?::\d{2})?\b/g;
const PORTA = /\bportas?\s+(\d{1,5}(?:\s*(?:,|e)\s*\d{1,5})*)/gi;

export function semApoioNosDados(factos, dados) {
  const texto = JSON.stringify(dados);
  const ips = new Set(texto.match(IP));
  const portas = new Set(texto.match(/"(\d{1,5}) /g)?.map((p) => p.slice(1, -1)));
  const falta = new Set();

  for (const f of factos) {
    for (const ip of f.match(IP) ?? []) if (!ips.has(ip)) falta.add(ip);
    // As horas nos dados vêm com dois dígitos ("09:05:12"); o modelo pode escrever "9:05".
    for (const h of f.match(HORA) ?? []) if (!texto.includes(h.replace(/^(\d):/, '0$1:'))) falta.add(h);
    for (const [, lista] of f.matchAll(PORTA)) {
      for (const p of lista.split(/\s*(?:,|e)\s*/)) if (!portas.has(p)) falta.add(`porta ${p}`);
    }
  }
  return [...falta];
}
