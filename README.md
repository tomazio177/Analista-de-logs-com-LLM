# Analista de logs com LLM

Vigia a rede de casa: descobre que aparelhos estão ligados, que portas têm abertas, guarda o histórico e diz o que mudou desde a última vez. A fase seguinte põe um LLM a fazer a triagem dessas mudanças.

Escrito em Node.js puro, sem dependências — o scanner, a descoberta de aparelhos e a base de dados usam só módulos do próprio Node.

> **Só contra redes tuas.** Varrer redes alheias sem autorização é crime (em Portugal, Lei do Cibercrime, Lei 109/2009). Este projeto existe para conhecer e proteger a rede de casa.

## Como correr

```bash
npm run scan
```

Precisa de Node 22.13 ou mais recente. O primeiro varrimento fica guardado como linha de base; os seguintes mostram só o que mudou.

## Como funciona

1. **Descobrir a rede** (`src/rede.js`) — lê as interfaces do sistema, ignora adaptadores virtuais (Docker, WSL, VPNs) e calcula os endereços da rede local. Redes maiores que /24 ficam limitadas a 254 endereços.
2. **Descobrir aparelhos** (`src/descobrir.js`) — ping a cada endereço, 48 em paralelo, e depois a tabela ARP do sistema. A tabela ARP apanha os aparelhos que ignoram o ping: para o ping sair, o PC teve de perguntar quem tinha aquele IP, e a resposta fica registada.
3. **Testar portas** (`src/portas.js`) — *connect scan* a 18 portas escolhidas pelo que revelam num aparelho de casa (telnet, SMB, RDP, MQTT, câmaras RTSP, impressoras…). Não precisa de privilégios de administrador.
4. **Guardar e comparar** (`src/historico.js`, `src/comparar.js`) — SQLite com três tabelas (varrimentos, dispositivos, observações). A comparação deteta aparelhos novos, que voltaram, que saíram, que mudaram de IP, e portas que abriram ou fecharam.

## Limitações conhecidas

Estas são as fontes de falsos positivos e falsos negativos — e a parte mais interessante do projeto.

- **MACs aleatórios.** iPhones e Android usam um MAC privado por rede. Normalmente é estável, mas pode rodar e aparecer como "aparelho novo". O programa reconhece estes MACs (bit "administrado localmente") e avisa no próprio alerta, mas não consegue saber de que aparelho se trata.
- **Aparelhos a dormir.** Telemóveis em repouso e aparelhos IoT com Wi-Fi intermitente podem não responder e aparecer como "saiu".
- **Connect scan é ruidoso e lento.** Um scan SYN (como o do nmap) seria mais rápido, mas precisa de pacotes em bruto e privilégios de administrador.
- **Só 18 portas.** Um serviço numa porta fora da lista passa despercebido.
- **Sistema pelo TTL.** É uma adivinha: cerca de 64 costuma ser Linux, Android ou iPhone; cerca de 128, Windows; cerca de 255, routers e afins. Não distingue Android de iPhone, e aparelhos que só aparecem pela tabela ARP ficam sem TTL.
- **Nomes.** Dependem do router. Em alguns routers domésticos o DNS inverso devolve o nome de cada aparelho (`.lan`); noutros routers a maioria aparece sem nome.

## Roteiro

- [x] **Fase 1** — inventário da rede, histórico em SQLite, comparação entre varrimentos
- [ ] **Fase 2** — registos do Windows (logins falhados, evento 4625) e triagem das mudanças com um LLM, resposta validada com Zod
- [ ] **Fase 3** — varrimento automático de hora a hora, alertas só quando há algo estranho, painel web

## O que aprendi

*A preencher ao longo do projeto.*
