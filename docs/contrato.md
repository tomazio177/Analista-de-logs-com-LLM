# Contrato para outros programas

A interface (a app em C++, ou outra qualquer) não repete nada do que o motor faz: corre os comandos com `--json` e lê o resultado. As regras, a triagem e a base de dados ficam todas do lado do Node, que é onde estão os testes.

## Como chamar

```
node --disable-warning=ExperimentalWarning <pasta do projeto>/src/<comando>.js [opções] --json
```

- Chama o `node` diretamente. Pelo `npm run` não dá: o npm escreve as linhas `> analista-de-logs-com-llm@… scan` no stdout, antes do JSON (`npm run --silent` também evita isso).
- A pasta de trabalho não importa: a base de dados e o `.env` são procurados a partir da pasta do projeto.
- Os dados (base de dados e `.env` com a chave) ficam na pasta do utilizador: `%APPDATA%\Analista de logs` no Windows, `~/.local/share/analista-de-logs` no Linux, `~/Library/Application Support/Analista de logs` no macOS. `ANALISTA_DADOS=<pasta>` troca essa pasta e `ANALISTA_DB=<caminho>` troca só a base de dados. Uma base antiga em `data/analista.db` é copiada para lá na primeira vez.
- O stdout traz **um só** documento JSON, numa linha, em UTF-8. O código de saída é 0 quando `ok` é `true` e 1 nos outros casos.
- O `scan` e o `vigiar` escrevem o andamento no **stderr**, um objeto por linha: `{"progresso":{"fase":"inicio",…}}`, `{"progresso":{"fase":"descobrir","feitos":120,"total":254}}`, `{"progresso":{"fase":"portas","aparelhos":7,"portas":18}}`. O `vigiar` escreve ainda `{"progresso":{"fase":"triagem","dados":{…}}}` antes de chamar o modelo, com os dados que vão seguir. Serve para uma barra de progresso; pode ser ignorado.

## O documento

```json
{ "contrato": 1, "ok": true, … }
{ "contrato": 1, "ok": false, "erro": { "codigo": "rede-por-aprovar", "mensagem": "Rede nova: …" }, … }
```

- `contrato` sobe quando um campo muda de nome ou de significado, ou desaparece. Campos novos não o fazem subir: ignora os que não conheces.
- Decide pelo `codigo`, nunca pela `mensagem`: as mensagens são para pessoas e podem mudar.
- As horas vêm em ISO 8601 UTC (`2026-10-22T14:45:00.000Z`), exceto dentro de `dados` da triagem, que vai para o modelo em hora local.

## Comandos

| Comando | Opções | O que devolve quando corre bem |
|---|---|---|
| `index.js` (varrer a rede) | `--aprovar` | `rede`, `interface`, `ip_deste_pc`, `router {ip, mac}`, `aprovada_agora`, `anterior` (data do varrimento anterior ou `null`), `aparelhos[]`, `mudancas[]` |
| `logs.js` (logins falhados) | dias como primeiro argumento (1–365, por omissão 7) | `dias`, `registo_mais_antigo`, `total`, `novos`, `logins[]`, `alertas[]`, `periodos_possivelmente_incompletos[]` |
| `triar.js` (triagem) | `--ver-dados` devolve só `dados`, sem enviar nada; `--ultima` devolve a última triagem guardada, sem chamar a API (é o que a app abre ao arrancar) | `triagem_id`, `data`, `modelo`, `uso {entrada, saida}` (`null` na última guardada), `nivel_geral`, `resumo`, `dados` (o que seguiu para o modelo, com os ids `d1`, `a1`… que os achados citam), `achados[]`, `avisos[]`, `perguntas[]` |
| `notas.js --perguntas` | | `triagem {id, data}`, `perguntas[]` da última triagem |
| `notas.js --pergunta N --texto=…` | Escreve o texto na mesma peça que `--texto=`: assim uma resposta que comece por `-` não é tomada por outra opção | `resposta_id` |
| `notas.js --listar` | | `respostas[]` |
| `notas.js --apagar N` | | nada além de `ok` |
| `config.js` | | `pasta_de_dados`, `chave_definida` |
| `config.js --guardar-chave` | A chave entra pelo **stdin**, nunca na linha de comandos, onde outros programas a podiam ver. O motor confirma-a com a Anthropic antes de a guardar | `pasta_de_dados`, e `aviso` se não deu para confirmar (sem internet: fica guardada na mesma) |
| `logs.js --dados=<pasta>` | Para quando corre como administrador: o Windows não passa as variáveis de ambiente a um processo elevado | o mesmo que `logs.js` |
| `vigiar.js` (tudo de seguida) | `--aprovar`; `--sem-logs` salta a leitura dos logins (a app lê-os à parte, como administrador) e devolve `logins: null` | `varrimento`, `logins` e `triagem`: os três resultados acima, cada um com o seu `ok`. O `ok` de fora é o da triagem |

Os objetos dentro das listas:

- **aparelho**: `ip`, `mac`, `nome` (ou `null`), `este_pc`, `ttl` (ou `null`), `sistema` (adivinhado pelo TTL), `portas[] {porta, servico}`.
- **mudança**: `tipo` (`novo`, `voltou`, `desapareceu`, `mudou-ip`, `portas-abertas`, `portas-fechadas`), `ip`, `mac`, `nome`, e conforme o tipo `ip_anterior`, `portas[]`, `mesmo_nome_que`.
- **login**: `quando`, `utilizador`, `tipo`, `motivo`, `origem` (IP, `este PC` ou `rede (IP desconhecido)`), `aparelho_de_origem {mac, nome}` (ou `null`), `processo`, `novo`.
- **alerta**: `tipo` (`rajada`, `pela-rede`, `utilizador-inexistente`), `origem`, `remoto`, `n`, `de`, `ate`, `gravidadeMinima` (ou `null` nos alertas deste PC), `aparelho_de_origem`; nas rajadas também `segundosEntreTentativas`, `ritmoDeMaquina` e `processos[]`; nos outros, `utilizadores[]`. Estes vêm das regras com os nomes que lá têm (em camelCase).
- **achado**: `titulo`, `gravidade` (`info`, `baixa`, `media`, `alta`), `gravidade_do_modelo` (só quando as regras a subiram), `alertas_relacionados[]`, `nos_dados[]`, `conhecimento_geral[]`, `conclusao`, `acao_sugerida`, `confianca`, `factos_sem_apoio[]` (IPs, horas e portas citados como factos que não estão nos dados: mostra-os como aviso).
- **pergunta**: `n` (o número a usar em `--pergunta`), `pergunta`, `sobre {tipo, alvo}` (ou `null`), `respondida`, `resposta {texto, data}` (a resposta mais recente do dono sobre o mesmo aparelho ou episódio, dada nesta triagem ou numa anterior; `null` se não houver).
- **resposta**: `id`, `data`, `sobre {tipo, alvo}` (ou `null`), `pergunta`, `texto`.

## Códigos de erro

| Código | Quando | O que a app pode fazer |
|---|---|---|
| `rede-por-aprovar` | A rede ainda não foi aprovada; não se varreu nada | Mostrar o router e pedir confirmação; se sim, repetir com `--aprovar` |
| `sem-router` | Não se conseguiu saber o router da rede | Avisar; nada é varrido sem router |
| `sem-rede` | Não há nenhuma ligação de rede IPv4 | Avisar |
| `sem-permissao` | Ler os logins precisa de administrador | Oferecer correr como administrador |
| `nao-windows` | Os logins só existem no Windows | Esconder a parte dos logins |
| `dias-invalidos` | Os dias não são um inteiro de 1 a 365 | Corrigir o pedido |
| `sem-varrimentos` | Não há varrimentos para a triagem | Pedir um varrimento primeiro |
| `sem-chave` | Falta a `ANTHROPIC_API_KEY`; o campo `detalhe` diz o que está mal no `.env` | Pedir a chave e guardá-la com `config.js --guardar-chave` |
| `chave-vazia` | Ao guardar: a chave está vazia | Pedir outra vez |
| `chave-invalida` | Ao guardar: não começa por `sk-ant-` | Pedir outra vez |
| `chave-recusada` | A API recusou a chave (ao guardar, ou na triagem) | Pedir outra chave |
| `limite-de-pedidos` | Demasiados pedidos à API | Tentar mais tarde |
| `sem-ligacao` | Sem ligação à API | Verificar a internet |
| `erro-da-api` | Outro erro da API | Mostrar a mensagem |
| `recusa` | O modelo recusou fazer a triagem | Mostrar a mensagem; fica guardado |
| `resposta-cortada` | A resposta passou o limite de tokens | Tentar outra vez |
| `resposta-invalida` | A resposta do modelo não respeita o esquema | Tentar outra vez; o texto fica guardado na tabela `triagens` |
| `sem-triagem` | Ainda não há triagem com resposta | Pedir uma triagem primeiro |
| `pergunta-inexistente` | O número da pergunta não existe na última triagem | Atualizar a lista de perguntas |
| `resposta-vazia` | A resposta está vazia | Pedir texto |
| `resposta-inexistente` | Não há resposta com esse número para apagar | Atualizar a lista |
| `opcao-invalida` | Opção desconhecida ou mal escrita | Erro da app: corrigir a chamada |

Os nomes dos aparelhos e dos utilizadores vêm da rede, tal como chegaram: qualquer pessoa pode dar ao telemóvel um nome com caracteres de controlo ou que invertem o texto (U+202E). Quem os mostrar tem de os tratar.
