// Corre `fn` sobre todos os itens, com no máximo `limite` em simultâneo.
// Sem isto, 254 pings ao mesmo tempo abrem 254 processos de uma vez.
export async function emParalelo(itens, limite, fn) {
  const resultados = new Array(itens.length);
  let proximo = 0;

  async function trabalhador() {
    while (proximo < itens.length) {
      const i = proximo++;
      resultados[i] = await fn(itens[i], i);
    }
  }

  await Promise.all(Array.from({ length: Math.min(limite, itens.length) }, trabalhador));
  return resultados;
}

export function comTimeout(promessa, ms, valorSeFalhar) {
  const espera = new Promise((resolve) => setTimeout(() => resolve(valorSeFalhar), ms));
  return Promise.race([promessa.catch(() => valorSeFalhar), espera]);
}
