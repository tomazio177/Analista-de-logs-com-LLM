// Quantos varrimentos seguidos sem resposta até dar um aparelho como "saiu".
// Um telemóvel em repouso falha um ping sem ter saído de casa; três falhas já são sinal.
export const LIMITE_AUSENCIA = 3;

// Compara o varrimento atual com o anterior e devolve só o que mudou.
// Esta lista é o que, na fase 2, vai para o modelo — não o inventário inteiro.
//
// `ausencias` diz, por MAC, quantos varrimentos anteriores falhou desde a última vez que foi visto
// (ver `ausenciasPorMac` em historico.js). Assim "saiu" e "voltou" só aparecem quando a ausência foi real.
export function compararVarrimentos(anterior, atuais, conhecidos, ausencias = new Map(), limite = LIMITE_AUSENCIA) {
  const mudancas = [];
  const vistosAgora = new Set(atuais.map((a) => a.mac));

  for (const a of atuais) {
    if (!conhecidos.has(a.mac)) {
      // Um telemóvel que trocou de MAC privado costuma manter o nome. É só uma pista: o nome pode repetir-se ou ser falso.
      const mesmoNome = a.nome && [...conhecidos].find(([, nome]) => nome === a.nome)?.[0];
      mudancas.push({ tipo: 'novo', aparelho: a, ...(mesmoNome && { mesmoNomeQue: mesmoNome }) });
      continue;
    }
    const faltas = ausencias.get(a.mac)?.faltas ?? 0;
    if (faltas >= limite) {
      mudancas.push({ tipo: 'voltou', aparelho: a });
      continue;
    }
    const antes = anterior?.aparelhos.get(a.mac);
    if (!antes) continue; // falhou alguns varrimentos mas nunca chegou a "sair": não é notícia

    if (antes.ip !== a.ip) {
      mudancas.push({ tipo: 'mudou-ip', aparelho: a, de: antes.ip });
    }
    const abriram = a.portas.filter((p) => !antes.portas.includes(p));
    const fecharam = antes.portas.filter((p) => !a.portas.includes(p));
    if (abriram.length) mudancas.push({ tipo: 'portas-abertas', aparelho: a, portas: abriram });
    if (fecharam.length) mudancas.push({ tipo: 'portas-fechadas', aparelho: a, portas: fecharam });
  }

  // Avisa uma única vez: no varrimento em que a ausência chega ao limite.
  for (const [mac, { faltas, ip }] of ausencias) {
    if (!vistosAgora.has(mac) && faltas + 1 === limite) {
      mudancas.push({ tipo: 'desapareceu', aparelho: { mac, ip } });
    }
  }

  return mudancas;
}
