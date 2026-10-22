// Compara o varrimento atual com o anterior e devolve só o que mudou.
// Esta lista é o que, na fase 2, vai para o modelo — não o inventário inteiro.
export function compararVarrimentos(anterior, atuais, conhecidos) {
  const mudancas = [];
  const vistosAgora = new Set(atuais.map((a) => a.mac));

  for (const a of atuais) {
    const antes = anterior?.aparelhos.get(a.mac);

    if (!conhecidos.has(a.mac)) {
      mudancas.push({ tipo: 'novo', aparelho: a });
      continue;
    }
    if (!antes) {
      mudancas.push({ tipo: 'voltou', aparelho: a });
      continue;
    }
    if (antes.ip !== a.ip) {
      mudancas.push({ tipo: 'mudou-ip', aparelho: a, de: antes.ip });
    }
    const abriram = a.portas.filter((p) => !antes.portas.includes(p));
    const fecharam = antes.portas.filter((p) => !a.portas.includes(p));
    if (abriram.length) mudancas.push({ tipo: 'portas-abertas', aparelho: a, portas: abriram });
    if (fecharam.length) mudancas.push({ tipo: 'portas-fechadas', aparelho: a, portas: fecharam });
  }

  for (const [mac, antes] of anterior?.aparelhos ?? []) {
    if (!vistosAgora.has(mac)) mudancas.push({ tipo: 'desapareceu', aparelho: { mac, ip: antes.ip } });
  }

  return mudancas;
}
