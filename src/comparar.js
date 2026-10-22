// Compara o varrimento atual com o anterior e devolve só o que mudou.
// Esta lista é o que vai para o modelo — não o inventário inteiro.
// `conhecidos` é um Map de MAC → portas da última vez que esse aparelho foi visto.
export function compararVarrimentos(anterior, atuais, conhecidos) {
  const mudancas = [];
  const vistosAgora = new Set(atuais.map((a) => a.mac));

  for (const a of atuais) {
    if (!conhecidos.has(a.mac)) {
      mudancas.push({ tipo: 'novo', aparelho: a });
      continue;
    }

    const antes = anterior?.aparelhos.get(a.mac);
    if (!antes) {
      mudancas.push({ tipo: 'voltou', aparelho: a });
    } else if (antes.ip !== a.ip) {
      mudancas.push({ tipo: 'mudou-ip', aparelho: a, de: antes.ip });
    }

    // Quem voltou compara as portas com a última vez que foi visto, não com o varrimento em que faltou:
    // senão, um aparelho que dormiu num varrimento e acordou com o telnet aberto passava sem aviso.
    const portasAntes = antes?.portas ?? conhecidos.get(a.mac);
    const abriram = a.portas.filter((p) => !portasAntes.includes(p));
    const fecharam = portasAntes.filter((p) => !a.portas.includes(p));
    if (abriram.length) mudancas.push({ tipo: 'portas-abertas', aparelho: a, portas: abriram });
    if (fecharam.length) mudancas.push({ tipo: 'portas-fechadas', aparelho: a, portas: fecharam });
  }

  for (const [mac, antes] of anterior?.aparelhos ?? []) {
    if (!vistosAgora.has(mac)) mudancas.push({ tipo: 'desapareceu', aparelho: { mac, ip: antes.ip } });
  }

  // O id liga cada mudança à avaliação do modelo: é por ele que se verifica que nenhuma ficou de fora.
  return mudancas.map((m, i) => ({ id: `m${i + 1}`, ...m }));
}
