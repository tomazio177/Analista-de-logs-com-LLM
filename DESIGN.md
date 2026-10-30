---
name: Analista de logs
description: Um instrumento de laboratório. Os dados são o material; o Claude é o feixe de luz que os lê.
colors:
  substrate: "#070B14"
  surface: "#0C1220"
  surface-raised: "#111A2C"
  hairline: "rgba(255,255,255,0.06)"
  hairline-strong: "rgba(255,255,255,0.10)"
  text: "#E6EDF7"
  text-muted: "#7D8AA0"
  text-faint: "#4A5568"
  claude: "#3DE0FF"
  claude-dim: "rgba(61,224,255,0.14)"
  severity-error: "#FF4D4D"
  severity-warning: "#FFB020"
typography:
  numeral:
    fontFamily: "Geist"
    fontSize: "56px"
    fontWeight: 700
    lineHeight: 1
    letterSpacing: "-0.02em"
  heading:
    fontFamily: "Geist"
    fontSize: "15px"
    fontWeight: 700
    lineHeight: 1.3
  body:
    fontFamily: "Geist"
    fontSize: "13px"
    fontWeight: 400
    lineHeight: 1.5
  log:
    fontFamily: "JetBrains Mono"
    fontSize: "12px"
    fontWeight: 400
    lineHeight: 1.55
  meta:
    fontFamily: "JetBrains Mono"
    fontSize: "10px"
    fontWeight: 400
    letterSpacing: "0.12em"
rounded:
  base: "4px"
spacing:
  hair: "1px"
  xs: "4px"
  sm: "8px"
  md: "12px"
  lg: "20px"
  xl: "32px"
components:
  button:
    backgroundColor: "{colors.surface-raised}"
    textColor: "{colors.text}"
    rounded: "{rounded.base}"
    padding: "0 14px"
    height: "30px"
  button-primary:
    backgroundColor: "{colors.text}"
    textColor: "{colors.substrate}"
    rounded: "{rounded.base}"
    padding: "0 16px"
    height: "30px"
  citation-chip:
    backgroundColor: "{colors.claude-dim}"
    textColor: "{colors.claude}"
    typography: "{typography.meta}"
    rounded: "{rounded.base}"
    padding: "2px 6px"
  evidence-row:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.text}"
    typography: "{typography.log}"
    height: "20px"
  input:
    backgroundColor: "{colors.substrate}"
    textColor: "{colors.text}"
    rounded: "{rounded.base}"
    height: "30px"
---

# Analista de logs — sistema visual

> Fonte única dos tokens no código: `app/src/tema.h` / `tema.cpp`. Nenhum hex fora desses dois ficheiros.
> Escrito antes da implementação por pedido explícito do dono do projeto; será revisto no fim, a partir do que for construído.

## Overview

**Conceito: um instrumento de laboratório.** Um substrato escuro e calmo onde os dados da rede e dos logins são o material, dispostos como uma lâmina num microscópio, e o Claude é um feixe de luz cyan que os lê. A janela é reconhecível por três coisas, mesmo sem o nome da app: a coluna de evidência densa em mono com marcas cyan na margem, o número grande e pesado à direita, e os chips cyan `L014` que ligam cada conclusão à sua linha.

**Modo: Operate.** Quem abre quer saber em menos de um segundo se há gravidade, e em menos de dez porquê. A expressão vive nos detalhes de precisão (hairlines, tracking, a luz do feixe), não em decoração.

**Cena física:** o dono, em casa, ao fim do dia, num portátil ou monitor com a luz da sala baixa, depois de um aviso ou por rotina. Escuro, porque é um instrumento de leitura prolongada de texto denso, e porque a luz do feixe só existe sobre fundo escuro.

**Rejeitado:** tema escuro genérico com cartões arredondados e sombra; neon espalhado; cyan como cor de marca da app; grelhas bento; hambúrgueres; navbars em pílula; "scroll reveals"; vermelho decorativo.

### Composição da janela principal

```
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│ ANALISTA DE LOGS   REDE 192.168.1.0/24 · TRIAGEM 30/10 21:14      ● CLAUDE A LER   [Verificar agora] [⚙] │  barra (surface)
├──────────────────────────────────────────┬───────────────────────────────────────────────┤  hairline
│ EVIDÊNCIA · 214 LINHAS            ░ │                                               │
│ 001 │ D1  192.168.1.1   router …     ░ │  ATENÇÃO                                       │
│ 002 │ D2  192.168.1.20  tv-sala …    ░ │  3        achados que pedem atenção           │  respira
│ 014▐│ A1  RAJADA  este PC  12× …     █ │  ───────────────────────────────────────────   │
│ 015▐│ L   14:45:03  msedge.exe …     ▌ │  ▌ALTA  Tentativas vindas da TV da sala        │
│ ═══════════ feixe cyan ════════════ ░ │    conclusão em Geist 13…                      │
│ 016 │ L   …                          ░ │    [L014] [L015–L018]   confiança alta        │
│     │                                ▌ │  ▌MÉDIA …                                      │
│      denso, mono 12px, linhas de 20px  ↑minimapa│  PERGUNTAS QUE SÓ TU SABES RESPONDER           │
├──────────────────────────────────────────┴───────────────────────────────────────────────┤
│ faixas de estado/erro, quando as há (inline, por cima dos painéis)                       │
└──────────────────────────────────────────────────────────────────────────────────────────┘
   esquerda ~44% (QSplitter, mín. 360px)          direita ~56% (mín. 420px)
```

## Colors

Estratégia **Restrained**: um escuro dominante em três camadas de luminosidade e um único acento da mesma temperatura (azul-frio). O contraste faz-se por luminosidade, não por matiz.

### Neutral
- **Substrato `#070B14`**: o fundo da janela e da coluna de evidência (o "vidro" da lâmina).
- **Superfície `#0C1220`**: barra superior, painel de análise.
- **Superfície elevada `#111A2C`**: botões, campos, linha selecionada, faixas.
- **Hairline `rgba(255,255,255,0.06)`**: toda a separação entre compartimentos. `hairline-strong` (0.10) só para o foco do teclado e o rebordo dos campos.
- **Texto `#E6EDF7`** (contraste 17:1 no substrato). **Texto secundário `#7D8AA0`** (5,4:1 na superfície, 5,0:1 na superfície elevada: passa AA). **Texto ténue `#4A5568`** só para números de linha e elementos não essenciais (não é texto a ler).

### Primary — a voz do Claude
- **Cyan `#3DE0FF`**: exclusivo do Claude. Aparece no feixe, nas marcas de citação na margem e na minimapa, nos chips de citação, no indicador de estado quando o Claude está ativo, no cursor da revelação e no rótulo «CLAUDE». `claude-dim` (14%) é o fundo dos chips e o halo das linhas citadas.

### Severidade
- **Erro `#FF4D4D`**: só gravidade `alta` e nível `urgente`, e erros que impedem a triagem.
- **Aviso `#FFB020`**: gravidade `media`, nível `atenção`, avisos.
- **Info / baixa / tranquilo**: texto secundário. Não há verde: "tranquilo" é a ausência de alarme, dita em texto grande e calmo.

### Named Rules
- **A regra do feixe.** Se o cyan aparece, o Claude disse ou citou aquilo. Botões, foco, seleção e links da app nunca são cyan.
- **A regra da gravidade.** O vermelho e o âmbar só existem quando há gravidade real; um ecrã tranquilo não tem nenhum.

## Typography

Duas famílias, embebidas em `app/fontes/` (licença OFL, com `OFL.txt` copiado para o instalador) e carregadas com `QFontDatabase::addApplicationFont`. Nenhuma fonte do sistema (Segoe UI, Arial) aparece em lado nenhum, incluindo diálogos e menus.

- **JetBrains Mono — a que se cala.** Evidência, horas, IPs, portas, números de linha, metadados. Peso 400 apenas.
- **Geist — a que grita.** Texto de interface e da análise em 400; títulos e o número-chave em 700.
- Pesos por ecrã: Geist 400 + 700, Mono 400. Nunca um terceiro peso.

### Hierarchy
- **Numeral** (Geist 700, 56 px, tracking −2%): o número de achados que pedem atenção. Com «tranquilo», o numeral é `0` em texto secundário.
- **Nível** (Mono 10 px, maiúsculas, tracking 0,12em, na cor da gravidade): `URGENTE`, `ATENÇÃO`, `TRANQUILO`.
- **Heading** (Geist 700, 15 px): título de cada achado.
- **Body** (Geist 400, 13 px, 1,5): resumo, conclusões, ações. Medida máxima ~72 caracteres.
- **Log** (Mono 400, 12 px, linhas de 20 px): cada linha de evidência.
- **Meta** (Mono 400, 10 px, maiúsculas, tracking 0,12em): rótulos de secção («NOS DADOS», «CONHECIMENTO GERAL»), cabeçalho dos painéis, estado do Claude, chips.

Tamanhos em pixels lógicos; o Qt escala com o DPI. `QFont::PreferAntialias` e `QFont::PreferNoHinting` para o mesmo desenho a 100% e 150%.

## Layout

- **Densidade bimodal.** A coluna de evidência é densa: linhas de 20 px, sem espaço entre elas, gutter de 44 px com número de linha e marca. O cabeçalho da análise respira: 32 px acima e abaixo do numeral.
- **Topologia fixa**: barra superior (44 px) · faixas (só quando há) · `QSplitter` horizontal com evidência à esquerda e análise à direita. O divisor é uma hairline de 1 px com área de agarrar de 6 px.
- Tamanho inicial 1280×800; mínimo 900×600. Abaixo de 1040 px de largura, a evidência encolhe até 360 px antes de a análise encolher.
- Grelha de 4 px. Margens internas dos painéis: 20 px na análise, 0 na evidência (a lista vai de borda a borda; o texto começa depois do gutter).

## Elevation & Depth

Não há sombras. A profundidade vem de três luminosidades de superfície e de hairlines. A única luz da janela é o feixe.

- **O feixe**: uma linha de 1 px cyan a 90% com um halo vertical de 28 px pintado como `QLinearGradient` (cyan 18% → 0%) por cima e por baixo, e uma leve intensificação nas linhas que atravessa. Pintado numa camada transparente sobre o viewport da lista, que só repinta o retângulo antigo + o novo.
- Grão ou ruído, se algum: só na barra superior (superfície fixa). Nunca na lista nem no painel de análise, que fazem scroll.

## Shapes

Um só raio: **4 px**, em botões, campos, chips e faixas. Compartimentos (painéis, barra) têm raio 0 e são separados por hairlines. As marcas de gravidade dos achados são réguas verticais de 2 px, não pílulas.

## Components

### Buttons
Pintados à mão (`QAbstractButton` com `paintEvent`), porque o QSS não anima.
- **Primário («Verificar agora»)**: fundo `text`, texto `substrate`, Geist 700 13 px. É o único botão claro da janela: a ação principal é a luz mais forte que não é o Claude.
- **Secundário** (Guardar, Aprovar…): `surface-raised`, texto `text`, hairline-strong.
- **Ícone** (definições): 30×30, só traço.
- Estados animados com `QVariantAnimation` 140 ms `OutCubic`: hover clareia o fundo 6%, pressão escurece 4%, desativado desce a opacidade para 40%. Foco do teclado: anel de 1 px `hairline-strong` + 1 px de folga.

### Coluna de evidência (componente-assinatura)
- `QListView` + modelo próprio (`QAbstractListModel`) + `QStyledItemDelegate` que pinta à mão; `setUniformItemSizes(true)`, sem widgets por linha.
- Cada linha: `001` (Mono, texto ténue) · marca de 2 px no gutter · etiqueta de tipo (`D` aparelho, `A` alerta, `L` login, `N` nota do dono, `R` rede) · campos em colunas fixas por tipo. IPs e horas em `text`, o resto em `text-muted`.
- Marcas no gutter: erro/aviso (régua de gravidade) quando a linha é um alerta com gravidade; cyan quando o Claude cita a linha. Uma linha citada e grave mostra as duas (gravidade à esquerda, cyan à direita).
- Cabeçalho do painel em Meta: `EVIDÊNCIA · 214 LINHAS · ENVIADA AO CLAUDE ÀS 21:13`.
- **Minimapa**: a scrollbar vertical é um widget próprio de 10 px que desenha marcas de 2 px por linha com gravidade e por linha citada (cyan), escaladas à altura total, mais a janela visível como retângulo hairline. Clicar salta para lá.

### O feixe de leitura
- Enquanto o Claude lê (da fase «triagem» até chegar a resposta): varrimento de cima a baixo da área visível em 1,8 s com `InOutSine`, pausa de 300 ms, recomeça. 60 fps via `QVariantAnimation`; cada frame chama `update(QRect)` só na faixa antiga e na nova.
- Quando a resposta chega: o feixe faz uma última passagem `OutExpo` (600 ms) e apaga-se; as marcas cyan das linhas citadas acendem em cascata (40 ms entre elas, 180 ms de fade cada).
- **Reduzir movimento**: sem varrimento nem cascata. O estado «a ler» mostra-se no indicador da barra; as marcas aparecem de uma vez com um fade de 120 ms.

### Chips de citação
- `L014`, `L015–L018` (intervalos fundem linhas contíguas). Mono 10 px, cyan sobre `claude-dim`, raio 4 px.
- Clicar: scroll animado (`OutCubic`, 320 ms) até à primeira linha, que fica centrada; as linhas citadas acendem com halo `claude-dim` que se apaga em 1,2 s. Tab percorre os chips; Enter ativa.
- Origem das citações (decidido pelo dono): calculadas na app, sem mexer no prompt. `alertas_relacionados` (`a1`…) aponta para a linha do alerta; IPs, horas, portas e ids citados em `nos_dados` são procurados nas linhas.
- Um achado sem nenhuma linha encontrada mostra, no lugar dos chips, `SEM LINHA ENCONTRADA` em Meta, texto secundário: a interface não esconde que não conseguiu ligar a conclusão aos dados.

### Cabeçalho de resultados
- Nível em Meta na cor da gravidade; numeral grande; à direita do numeral, em Body secundário, o que conta («achados que pedem atenção», «achados, nenhum grave»). Por baixo, o resumo do Claude em Body com um traço cyan de 2 px à esquerda: é a voz do Claude.
- Metadados em Meta: `TRIAGEM 30/10 21:14 · CLAUDE OPUS · 3 ACHADOS · 2 PERGUNTAS`.

### Achados
- Sem caixa. Régua de 2 px à esquerda na cor da gravidade (secundária para baixa/info), título em Heading, gravidade em Meta.
- Secções «NOS DADOS», «CONHECIMENTO GERAL (NÃO VEM DOS TEUS DADOS)», «CONCLUSÃO», «O QUE FAZER» com rótulos em Meta. A conclusão é a voz do Claude: traço cyan à esquerda.
- Chips de citação + `CONFIANÇA ALTA` em Meta numa linha.
- Gravidade subida pelas regras e factos sem apoio: linha em Meta âmbar, com o texto atual.
- Separados por hairlines e 20 px.

### Perguntas
- Pergunta em Body 700; campo `input` + botão secundário «Guardar». Já respondida: `✓ JÁ RESPONDESTE` em Meta + a resposta entre aspas angulares em Body secundário.

### Indicador de estado do Claude (barra superior)
Sempre visível. Ponto de 6 px + rótulo em Meta.
| Estado | Ponto | Rótulo |
|---|---|---|
| Inativo | hairline-strong | `CLAUDE · INATIVO` (ou a hora da última triagem) |
| A preparar (rede e logins) | texto secundário, pulso lento | `A VARRER A REDE · 120/254` |
| A ler | cyan, pulso | `CLAUDE A LER · 214 LINHAS` |
| A revelar | cyan fixo | `CLAUDE A RESPONDER` |
| Erro da API / limite | erro | `CLAUDE INDISPONÍVEL` — e uma faixa com a frase clara e o que fazer |

### Revelação das conclusões
Quando o JSON chega, os achados entram um a um (180 ms de fade + 8 px de subida, `OutCubic`, 90 ms entre eles); a conclusão do achado ativo escreve-se com um cursor cyan de 2 px que pisca (530 ms) e desaparece no fim. Qualquer clique ou tecla salta para o fim. É apresentação do texto que já chegou inteiro, não streaming; com Reduzir movimento, aparece tudo de uma vez.

### Faixas (erros e avisos)
Inline, por baixo da barra, nunca diálogos para erros. Superfície elevada, régua de 2 px à esquerda na cor da gravidade (erro/aviso) ou texto secundário (info), texto em Body, ação opcional como botão secundário. Entram com altura animada 160 ms `OutCubic`. Sem stack traces: a mensagem vem do mapa de `codigo` → frase + ação.

### Estados vazios
- **Primeiro arranque (sem triagem)**: a coluna de evidência mostra um exemplo esbatido (40%) de 8 linhas com o rótulo `EXEMPLO` em Meta; o painel de análise mostra o numeral `—`, uma frase («Ainda não verificaste esta rede.») e três passos numerados em Mono: varre a rede · lê os logins falhados · o Claude explica e mostra onde. O botão primário está à vista e é o único caminho.
- O briefing pedia "arrastar um ficheiro de log": não se aplica, porque a app não lê ficheiros (lê a rede e o Windows). Decidido com o dono.

### Definições
Diálogo pequeno (botão ⚙): «Reduzir movimento» (por defeito segue a opção de animações do Windows) e «Chave da API…». Guardado com `QSettings`.

## Movimento

- Curvas: `OutCubic` para respostas a ações, `OutExpo` para chegadas, `InOutSine` só no ciclo do feixe. Nunca `Linear`.
- Durações: 120–180 ms em micro-interações, 320 ms em scroll, 600 ms em chegadas. Nenhuma mudança de estado instantânea num elemento interativo.
- Todas as animações passam por `Tema::animar(...)`, que devolve a duração 0 (ou 120 ms de fade) com Reduzir movimento.

## Do's and Don'ts

### Do:
- Ler todos os tokens de `Tema`; gerar o QSS a partir deles num só sítio.
- Pintar à mão o que tem de animar ou de ter profundidade (botões, lista, feixe, minimapa).
- Mostrar sempre onde está a evidência de cada conclusão, ou dizer que não foi encontrada.
- Testar a 100% e 150% (`QT_SCALE_FACTOR=1.5` na captura).

### Don't:
- Cyan em botões, foco, seleção ou qualquer coisa que não seja o Claude.
- Vermelho ou âmbar sem gravidade.
- `QGraphicsDropShadowEffect`, sombras, cartões com raio e borda.
- Um widget por linha de evidência.
- Animações lineares ou ignorar Reduzir movimento.
- Mais de duas famílias ou de dois pesos de Geist num ecrã.
- Mostrar a chave da API, stack traces ou texto vindo da rede interpretado como HTML.
