# Product

<!-- impeccable:product-schema 1 -->

## Platform

desktop

(Windows, Qt 6 Widgets. Não é `web` nem `ios`/`android`: as referências `*.native.md` aplicam-se pelos princípios — afordâncias nativas, Reduzir movimento, escala do sistema — e não pelas regras de iOS/Android.)

## Users

O dono de uma rede de casa, no PC dele (Windows), que quer saber se alguma coisa na rede ou nos logins do PC merece atenção. Não é analista de segurança: lê em português simples, abre a app de vez em quando ou depois de algo estranho, e quer uma resposta em segundos: "está tudo bem?" ou "o que tenho de ver?".

## Product Purpose

Vigiar a rede de casa e o PC: descobrir aparelhos e portas abertas, ler os logins falhados do Windows (evento 4625), guardar o histórico e pedir ao Claude uma triagem que explica o que merece atenção. Sucesso: o dono percebe o nível geral num relance e consegue confirmar cada conclusão nos dados que a sustentam.

## Positioning

O Claude não é a fonte da verdade: as regras decidem o que é alerta e o modelo explica. Cada achado separa o que está nos dados (`nos_dados`) do que o modelo sabe de fora (`conhecimento_geral`), e os factos citados que não estão nos dados aparecem assinalados. A interface torna esta separação visível: cada conclusão aponta para as linhas de evidência de onde vem.

## Operating Context

- A app (C++/Qt) não calcula nada: corre o motor em Node pelo contrato JSON (`docs/contrato.md`) e mostra o que ele devolve.
- Fluxo: abrir → mostra a última triagem guardada (sem gastar a API) → «Verificar agora» → varrimento da rede (com progresso), leitura dos logins (num processo elevado à parte), triagem pelo Claude (~1 minuto, resposta estruturada inteira, sem streaming).
- Volume real dos dados: dezenas de aparelhos, dezenas a centenas de logins falhados agrupados, poucos alertas. A interface tem de aguentar muito mais (100k linhas) sem engasgar.
- Interação de volta: o dono responde às perguntas da triagem na própria janela; a resposta fica presa ao aparelho ou ao alerta.

## Capabilities and Constraints

- Estados do motor e erros por `codigo` (rede-por-aprovar, sem-permissao, sem-chave, limite-de-pedidos, sem-ligacao, chave-recusada, recusa, resposta-cortada, resposta-invalida…). A interface decide pelo código, nunca pela mensagem.
- Texto vindo da rede (nomes de aparelhos e utilizadores) é sempre texto simples, limpo de caracteres de controlo e marcas bidi.
- A chave da API nunca aparece na interface nem em logs; segue pelo stdin.
- A lógica de análise e os prompts não mudam sem pedido explícito do dono do projeto.
- Varrer só redes próprias (Lei 109/2009): a aprovação de uma rede nova é uma confirmação explícita.

## Brand Commitments

- Nome: «Analista de logs». Língua: português de Portugal, frases simples, sem jargão.
- Ícone existente: `app/icone/analista.png` / `.ico`.
- Papel exclusivo de cor para a voz do Claude (definido em DESIGN.md).

## Evidence on Hand

- Dados de exemplo usados nos testes e em `docs/app-exemplo.png` (uma casa inventada). Não usar dados reais da rede de casa em capturas ou testes.
- Não há testemunhos, clientes nem números de utilização: não inventar.

## Product Principles

1. **Mostra onde está.** Uma conclusão sem a evidência à vista vale menos do que a evidência sem conclusão.
2. **As regras mandam, o modelo explica.** A gravidade mínima das regras é visível quando o modelo a quis baixar.
3. **Honestidade de estado.** A interface nunca finge: não simula streaming que não existe nem esconde que uma parte falhou.
4. **Calma por defeito.** O alarme visual é reservado a gravidade real.

## Accessibility & Inclusion

- Contraste WCAG AA no texto.
- Opção «Reduzir movimento» (por defeito segue a definição de animações do Windows).
- Escala do Windows a 100% e 150% sem cortes nem desfoque.
- Tudo utilizável pelo teclado (Tab, Enter, atalhos para verificar e para saltar entre citações).
