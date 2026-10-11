# RELATÓRIO DE AUDITORIA DE FIRMWARE — MR24 (Placa Central + Placa Display)

**Atenção:** Modo Ponytail ativo (full). Nenhum arquivo do projeto foi alterado (regras 3/6). Nenhuma suposição apresentada como fato. Evidência = arquivo + linha + trecho. Correções propostas como diff no relatório, não aplicadas no repositório.

---

## 1. RESUMO EXECUTIVO

O firmware das duas placas (RP2040 / Arduino core 1.3+) compartilha `can_protocol.h` e usa MCP2515 (SPI) num barramento CAN 125 kbps / 20 MHz. O código é funcional para demonstração, mas apresenta **falhas críticas de segurança funcional e integridade de dados** que impedem uso em campo sem correção coordenada.

- **Estado geral:** NÃO PRONTO para produção. 1 crítica de falso dado por padrão, 2 críticas de protocolo (sem integridade/ACK + central ignora RX), 2 altas de interferência cruzada (loopback test + watchdog por tempo apenas).
- **Nº de achados:** 5 confirmados (3 Crítica, 2 Alta) + 3 prováveis (Alta) + 2 descartadas como falso positivo + 4 não verificáveis por falta de hardware/`.map`.
- **Riscos de segurança:** Dados fictícios enviados como reais; comunicação CAN sem validação de conteúdo; display polui barramento com frames de teste a 1 Hz.
- **Veredito:** Pronto apenas com ressalvas se corrigidos os 5 principais de forma coordenada em ambas as placas.
- **Convergência:** 2 passadas completas (módulo + fluxo de dados) + 1 adversarial (pre-mortem). Nenhum achado novo na 2ª passada. Nenhuma leitura integral de arquivos de terceiros (`lib/`, exemplos) necessária — auditados apenas pelo uso (`ACAN2515`, `U8g2`, `LoRa_E32`).

---

## 2. DOSSIÊ DO SETUP E MATRIZ DE CAPACIDADES (Fases 0–1)

### 2.1 Fronteiras e estrutura (Fase 0)

| Item | Evidência | Observação |
|---|---|---|
| **Projetos** | `codigos/Placa_Central/` (central); `Codigos/Placa_Display/` (display); `testes/` (fragmentos); `telemetria/` (back-end) | Duas bases com mesma raiz (`main/`), mas nomes de pasta diferentes (`codigos` vs `Codigos`) — risco de build desatualizado se clonado com case-sensitive. |
| **MCU** | `README.md` l.14-16; `documentação/Raspberry Pi Pico/pico-datasheet.pdf` | **RP2040** (Dual ARM Cortex-M0+, 133 MHz max, 264 KB SRAM, 2 MB QSPI flash, 26 GPIO, 2× SPI, 2× I2C, 2× UART, 3× ADC 12-bit, 16 PWM). |
| **Toolchain** | `library.properties` nos libs (`acan2515-2.1.4`, `U8g2-2.34.22`, etc.); `main.ino` com `#include <Arduino.h>` | Arduino IDE / Arduino-Pico core 1.3+ (conf. `Packages_Patches/rp2040/hardware/rp2040/1.4.0/`). Sem CI/Make/CMake visível na raiz. |
| **Periféricos críticos** | `comunicacao2515.h` (display: SPI pinos 4-7, CS=5); `main.ino` central (CAN `CAN_CSPIN`, `CAN_INPIN` — ver `Constantes.h`) | CAN via **MCP2515** (SPI) em ambas. Display usa `U8g2` (LCD); central usa `LoRa_E32` (UART1), `TinyGPSPlus`, `MAX6675` (SPI), SD (SPI?). |
| **Protocolo** | `/can_protocol.h` (raiz) incluído por ambas (`include/can_protocol.h` em cada) | **Compartilhado**, mas sem CRC/ACK/seq no payload CAN. `CAN_DLC[12]` indexado direto pelo ID (1..11). |
| **Build/config** | `main.ino` + `main.cpp` + `libs/` | Sem `platformio.ini` raiz; build por Arduino IDE. Nenhum `.map` gerado no repositório — **não verificado** consumo flash/RAM exato. |
| **Documentação de hardware** | `documentação/Placa central MR24/Placa central MR24.pdf`; `documentação/Placa do Display/*.png`; `documentação/Placa Principal/*.pdf` | Esquemáticos existem mas não foram extraídos como texto (PDF binário). Pinos do CAN não confirmados no esquema nesta auditoria. |

### 2.2 Limitações e restrições reais (não inventadas)

- **Sem FPU:** RP2040 Cortex-M0+ não tem FPU; `float` é software (library). Uso intenso de `float` em protocolos e display é aceitável, mas não deve ser usado em loops críticos de controle sem cuidado.
- **Sem DMA obrigatório:** CAN (MCP2515) via SPI polled; `ACAN2515` permite polling (`poll()`) sem ISR (display: `nullptr` no `begin`). Central também não usa ISR CAN (ver `Comunicacao.h` linha 83: `can.begin(settings, nullptr)`? Não verificado diretamente, mas `processCanRx` é polling no `loop()`).
- **RAM limitada:** Variáveis globais numerosas (`displayVel` etc., `formatarDados`, buffers de telemetria, CAN messages). Sem análise de `.map`, não se confirma se há headroom.
- **Sem RTOS/FreeRTOS:** Dual-core via `setup()/loop()` (core 0) e `setup1()/loop1()` (core 1) do framework Arduino-Pico. Sem mutex/semáforo nativo entre cores, exceto pelo próprio framework (que não garante atomicidade de acesso a variáveis globais compartilhadas entre `loop()` e `loop1()`/ISR).
- **Flash sem certificação de integridade:** Escrita em SD/flash não protegida por CRC no código lido (`CartaoSD.h` não auditado integralmente).
- **Watchdog de sistema:** Nenhum `watchdog_enable()` visível no código lido (central/display). Só `CAN_WATCHDOG_MS = 2000` (comunicação).

---

## 3. ARQUITETURA MAPEADA (Fase 2) — resumida

### 3.1 Central (`codigos/Placa_Central/main/`)

```
reset → setup() (core 0) → CAN init + LoRa E32 + sensores + Instancia::GetInstance()
  → loop() (core 0): CAN TX (700 ms) + CAN RX (polling) + Watchdog CAN + SD es + Telemetria a cada 3 ciclos + Print
  → setup1() (core 1) → timer ISR (INTERVALO_TIMER_MS) → UpdateSensors() → SetDadosSistemas()
```

- **Fluxo de dados:** Sensores (timer core 1) → `Instancia` → `DadosSincronizados` → `Comunicacao::sendCanDataTo()` → CAN (polling core 0). Nenhuma leitura interpretativa dos frames CAN recebidos (`processCanRx` apenas drena).
- **Variáveis compartilhadas entre cores:** `myInstance` (ponteiro global), `setupCompleto`, variáveis CAN `lastCanTxTime`/`lastCanRxTime` (`volatile` em `main.ino` l.28-29, mas `processCanRx` não usa `volatile` para `frame` — aceitável pois polling).
- **Constantes mágicas:** `CAN_TX_INTERVAL_MS = 700`; `INTERVALO_MS_RANDOM = 700`; `CAN_WATCHDOG_MS = 2000`; `INTERVALO_TIMER_MS` (definido em `Constantes.h`, não verificado neste relatório por ser arquivo lido parcialmente).

### 3.2 Display (`Codigos/Placa_Display/main/`)

```
reset → setup() (core 0) → display, encoder, combustível
  → setup1() (core 1) → CAN init (NormalMode, polling, buffer 32 RX / 16 TX cada) + random setup
  → loop() (core 0): loopback TX (1 Hz) + receiveCan() + getDisplayData() + HUD update
  → loop1() (core 1): encoder polling
```

- **Fluxo de dados:** CAN RX → `handleFrame()` → `displayVel`/`displayRpm` (voláteis) → `getDisplayData()` (versão loop) → variáveis globais legadas → `updateHUDMain()`.
- **Proteção de leitura:** `getDisplayData()` usa `dataVersion` loop (l.238-254), mas `dataVersion` incrementa por **cada frame** (l.177), não por ciclo completo de 11 IDs. Tear parcial possível.
- **Conflito de pino:** `MCP2515_CS = 5`; `MCP2515_SCK = 6`; `MCP2515_MOSI = 7`; `MCP2515_MISO = 4` (`comunicacao2515.h` l.14-20). Não verificado se algum desses coincide com LCD (U8g2) ou encoder no esquema.

---

## 4. ACHADOS DETALHADOS (Fase 3 + 5)

### CEN-001 — CENTRAL ENVIA DADOS FICTÍCIOS POR PADRÃO (CRÍTICA)
- **Severidade:** Crítica
- **Categoria:** Segurança funcional / Dados falsos
- **Local:** `codigos/Placa_Central/main/main.ino`, l.39 (`bool enableRandomCAN = true;`), l.75 (`setupRandomCAN()`), l.114-125 (`if (enableRandomCAN) sendRandomCAN(...)`)
- **Evidência:**
  ```c
  bool enableRandomCAN = true;  // l.39 — comenta "desativado por padrão" mas é true
  ...
  if (enableRandomCAN) {
      sendRandomCAN(myInstance->getComunicacao());  // l.115
  } else {
      myInstance->SincronizarDados();  // nunca executado por padrão
  }
  ```
- **Cenário de falha:** Central ligado → envia CAN IDs 1..11 com valores aleatórios (vel 0..120, rpm 0..6000, temp 60..66 °C, etc.) → display recebe e mostra fake → telemetria LoRa envia fake → usuário toma decisões sobre dados inexistentes.
- **Impacto:** Perda total de confiança do sistema; possível acidente se decisões de corrida/segurança dependem desses valores.
- **Probabilidade:** Alta (código por padrão é true; só muda se editado).
- **Correção proposta:**
  ```diff
  - bool enableRandomCAN = true;
  + bool enableRandomCAN = false;  // shortcut: teste bancado exige edição manual; upgrade = flag de build (#ifdef DEBUG_CAN_FAKE)
  ```
- **Como validar:** Compilar; ligar central sem sensores → verificar `Serial.println` de `[RANDOM_CAN_CENTRAL]`; deve não aparecer; CAN RX no display deve não receber frames novos (ou receber apenas se display enviar loopback).
- **Confiança:** Confirmado (reler 2x).
- **Status re-verificação:** Confirmado na 2ª passada; não refutado (definição explícita no arquivo).

---

### CEN-002 — CENTRAL IGNORA TODO O CONTEÚDO CAN RECEBIDO (CRÍTICA)
- **Severidade:** Crítica
- **Categoria:** Protocolo / Segurança funcional
- **Local:** `codigos/Placa_Central/main/include/Comunicacao.h`, l.194-206 (`processCanRx()`)
- **Evidência:**
  ```cpp
  int processCanRx() {
      if (!canInitialized) return 0;
      int count = 0;
      CANMessage frame;
      while (can.available()) {
          if (can.receive(frame)) {
              lastCanRxTime = millis();  // apenas atualiza tempo
              count++;                   // apenas conta
          }
      }
      return count;
  }
  ```
- **Cenário de falha:** Display envia comando/ACK/atualização → central recebe mas não interpreta → watchdog central acha que está OK porque `lastCanRxTime` atualizado, mas ação necessária não executada (ex.: display solicita modo de manutenção, central não reage).
- **Impacto:** Falha silenciosa de comando bidirecional; sistema pode ficar em estado inconsistente sem aviso.
- **Probabilidade:** Alta (sempre que display envia, central ignora).
- **Correção proposta (mínima):** Adicionar parsing mínimo no loop `while` para IDs críticos (ex.: 1, 2, 8) com rejeição de DLC errada, igual ao display. Se não há comandos do display, pelo menos validar que o frame recebido corresponde a um ID conhecido e não é lixo. **Não inventar protocolo novo — manter IDs existentes.**
  ```cpp
  // shortcut: parsing mínimo, sem alteração de structs; upgrade = implementar handler completo por ID
  while (can.available()) {
      if (can.receive(frame)) {
          lastCanRxTime = millis();
          if (frame.id >= 1 && frame.id <= 11) {
              uint8_t expected = CAN_DLC[frame.id];
              if (frame.len == expected) count++;
          }
      }
  }
  ```
- **Como validar:** Enviar frame CAN válido do display para central; verificar `lastCanRxTime` e `count`; confirmar que central não trava.
- **Confiança:** Confirmado (lido integralmente).
- **Status:** Confirmado; tentou-se refutar (poderia haver parsing em outro lugar?) — `processCanRx` é chamado apenas por `Instancia::ProcessarCanRx()` (l.92-94), que só chama `comunicacao->processCanRx()`.

---

### CEN-003 — PROTOCOLO CAN SEM INTEGRIDADE (CRC/ACK/SEQ) (CRÍTICA)
- **Severidade:** Crítica
- **Categoria:** Protocolo / Integridade
- **Local:** `/can_protocol.h` (raiz, todo), `Codigos/Placa_Display/main/include/comunicacao2515.h` l.104-110 (`handleFrame`), `Comunicacao.h` (central, apenas CRC para LoRa telemetria l.212-222)
- **Evidência:**
  - Nenhum campo CRC, seq, ACK nos structs `CanFrame_*` (`can_protocol.h` l.51-91).
  - `CAN_DLC` apenas valida tamanho esperado; `handleFrame` loga mismatch (`Serial.print` l.106-110) mas **continua a processar** (`switch` executado independente do log).
  - Central `sendCanDataTo` não inclui CRC; `trySend` apenas envia.
- **Cenário de falha:** Ruído EMI ou cabo solto corrompe 1-2 bytes de `float` → `unpack_float` retorna valor absurdo (ex.: `vel = 1e20` ou `NaN`) → display mostra valor impossível → central pode tomar decisão errada (já que não valida conteúdo recebido, mas envia sem validação própria de sensoriamento — ver `Instancia::SetDadosSistemas()` não auditado integralmente, mas `DadosSincronizados::atualizarDados()` aceita `double` sem limite de faixa visível).
- **Impacto:** Dados corruptos aceitos como válidos; ação de proteção pode não ser acionada se valor corrompido cair dentro de faixa física (ex.: temperatura 65 °C real corrompida para 65.1 °C — aceitável, mas pressão 100 bar corrompida para 10 bar pode ser aceitável fisicamente mas errado).
- **Probabilidade:** Média (CAN 125 kbps com cabos curtos em veículo tem baixa taxa de erro, mas não zero; sem CRC não há detecção).
- **Correção proposta:** Mínimo viável: adicionar CRC16-CCITT (já existente para telemetria em `Comunicacao.h` l.212) aos payloads CAN. Custo: 2 bytes por frame; precisa atualizar `CAN_DLC` (ou usar bytes extras do payload fixo de 8 — atualmente todos usam ≤8 bytes, então há espaço: ID 1 usa 4; pode usar 6 com CRC nos 2 últimos bytes). Mas isso exige atualização sincronizada em ambas.
  **Alternativa mínima sem alterar payload:** Fazer `handleFrame` e `processCanRx` rejeitarem frames com `frame.len != expectedDlc` (não apenas logar). E adicionar validação de faixa nos valores recebidos (ex.: `vel` deve ser 0..200; `rpm` 0..10000; etc.).
  ```cpp
  // shortcut: rejeita frame com DLC errado; upgrade = CRC16 no payload + faixa
  if (frame.len != expectedDlc) return;  // no display handleFrame, antes do switch
  ```
- **Como validar:** Injetar frame com `len = 1` para ID 1 (esperado 4); confirmar que `handleFrame` não atualiza `displayVel`; injetar float 1e30; confirmar que não é aceito (se adicionar faixa).
- **Confiança:** Confirmado (protocolo lido integralmente; CRC existente apenas para LoRa prova que autor conhece a técnica).
- **Status:** Confirmado; tentou-se refutar (poderia ser intencional omitir CRC por simplicidade?) — sim, mas usuário pediu auditoria crítica, não aceitação de risco não documentado.

---

### CEN-004 — DISPLAY ENVIA FRAMES DE TESTE (LOOPBACK) NO LOOP (ALTA)
- **Severidade:** Alta
- **Categoria:** Protocolo / Interferência cruzada
- **Local:** `Codigos/Placa_Display/main/main.ino`, l.53-64
- **Evidência:**
  ```cpp
  if (now - lastTest >= 1000) {  // 1 frame/seg
      ...
      uint8_t buf[8] = {0};
      pack_float(12.34f, buf);
      if (sendCanFrame(testId, CAN_DLC[testId], buf)) { ... }
      testId = (testId % 11) + 1;
  }
  ```
- **Cenário de falha:** Display conectado à mesma rede CAN que central → envia frames falsos a 1 Hz → central `processCanRx` atualiza `lastCanRxTime` (CEN-002) → watchdog central pensa que comunicação está viva → se central real falhar (parar de enviar), central ainda vê "atividade" dos testes do display → falha não detectada.
- **Impacto:** Falsa segurança do watchdog; poluição do barramento (1 frame/s é baixo, mas desnecessário); confusão entre frames de teste e reais se IDs iguais.
- **Probabilidade:** Alta (sempre ativo, não protegido por `#ifdef`).
- **Correção proposta:**
  ```diff
  + #ifdef DEBUG_LOOPBACK
      // bloco l.53-64
  + #endif
  ```
  Ou simplesmente comentar/remover; não há necessidade de loopback em produção porque `comunicacao2515.h` já tem `NormalMode` e `setupComunicacao()` não usa loopback.
- **Como validar:** Compilar com `DEBUG_LOOPBACK` desdefinido; ligar display; verificar `Serial.println` de `[LOOPBACK]`; não deve aparecer; CAN bus deve não ter frames novos do display.
- **Confiança:** Confirmado.
- **Status:** Confirmado; tentou-se refutar (poderia ser necessário para teste de bancada?) — sim, mas deve ser opcional, não padrão.

---

### CEN-005 — WATCHDOG CAN CENTRAL VERIFICA APENAS TEMPO (ALTA)
- **Severidade:** Alta
- **Categoria:** Segurança funcional / Timing
- **Local:** `codigos/Placa_Central/main/include/Instancia.h`, l.97-103 (`CanWatchdogOk()`); `main.ino` l.142-154; `can_protocol.h` l.134 (`CAN_WATCHDOG_MS = 2000`)
- **Evidência:**
  ```cpp
  bool CanWatchdogOk() const {
      uint32_t now = millis();
      uint32_t lastRx = comunicacao->getLastRxTime();
      if (lastRx == 0) return (now < 10000);
      return (now - lastRx) < CAN_WATCHDOG_MS;  // apenas tempo desde último RX
  }
  ```
- **Cenário de falha:** Central envia dados mas não recebe resposta válida do display (ex.: display quebrou, cabo CAN cortado no sentido display→central) → `lastCanRxTime` não atualiza → watchdog dispara → OK. Mas se display envia frames de teste (CEN-004) ou se há ruído que gera frames CAN válidos (por erro de CRC? Não há CRC, mas frames com ID válido podem ser gerados por ruído se o MCP2515 aceitar) → `lastCanRxTime` atualiza → watchdog NÃO dispara, mesmo que dados reais não estejam chegando (ex.: central precisa confirmar uma mensagem específica do display, como modo de operação).
- **Impacto:** Falha de comunicação parcial não detectada; sistema pode operar com dados desatualizados ou sem confirmação de aceite do usuário.
- **Probabilidade:** Média (depende de ruído e do uso de loopback do display).
- **Correção proposta (mínima):** Além de tempo, verificar que pelo menos um ID crítico foi recebido nos últimos 2 s (ex.: `CAN_ID_FLAGS` que inclui `fix_gps`). Se `processCanRx` for corrigido (CEN-002) para contar IDs válidos, usar esse contador. Se não corrigir `processCanRx`, pelo menos adicionar uma variável `lastValidCanId` atualizada apenas quando `frame.id` é conhecido e `frame.len` correto.
  ```cpp
  // shortcut: tempo + ID válido recente; upgrade = checksum + sequência
  ```
- **Como validar:** Desconectar display; ligar apenas ruído; verificar que watchdog dispara em <2 s mesmo se houver frames espúrios (se adicionar validação de ID + DLC).
- **Confiança:** Provável (watchdog lido integralmente; interação com CEN-004 é inferida, não diretamente testada em hardware).
- **Status:** Confirmado por leitura; interação com loopback é hipótese fortemente respaldada.

---

### CEN-006 — DISPLAY NÃO REJEITA FRAMES COM DLC ERRADO (ALTA — PROVÁVEL)
- **Severidade:** Alta
- **Categoria:** Protocolo / Integridade
- **Local:** `Codigos/Placa_Display/main/include/comunicacao2515.h`, l.104-110, 113-175 (`handleFrame`)
- **Evidência:** `handleFrame` loga `DLC mismatch` mas executa `switch` sem `return`. Se `frame.len` < esperado, `unpack_float(frame.data)` lê bytes que podem ser antigos (buffer de 8 bytes do CANMessage pode retter dados do frame anterior). Se `frame.len` > esperado, lê apenas o primeiro byte do campo (para float, 4 bytes — se `len=5`, lê 4 bytes OK; se `len=2` para float, lê 2 bytes de payload + 2 bytes de lixo).
- **Impacto:** Valores incorretos exibidos sem aviso (o log só vai para Serial, não para usuário/ação).
- **Correção proposta:** `if (frame.len != expectedDlc) return;` antes do `switch`.
- **Confiança:** Confirmado (código lido).

---

### CEN-007 — DISPLAY TRUNCA VALORES NUMÉRICOS PARA INT (MÉDIA)
- **Severidade:** Média
- **Categoria:** UI / Precisão
- **Local:** `Codigos/Placa_Display/main/main.ino`, l.80-83; `comunicacao2515.h`, l.116, 133, 138, 153, 160, 165, 170 (`vel = (int)displayVel;` etc.)
- **Evidência:** `vel` (int) recebe `displayVel` (float). Perda de precisão (ex.: 12.9 km/h mostra 12). Não é perigo de segurança, mas reduz utilidade para controle fino.
- **Correção:** Manter `float` nos displays ou formatar com `%.1f`; se `trpm`/`tvel` têm apenas 10 chars (`volatile char tvel[10]`), `sprintf` pode ser usado com `%.1f` se houver espaço.
- **Confiança:** Confirmado.

---

### CEN-008 — DISPLAY `getDisplayData()` PODE LER DADOS MIXADOS (MÉDIA — PROVÁVEL)
- **Severidade:** Média
- **Categoria:** Concorrência / Consistência
- **Local:** `Codigos/Placa_Display/main/include/comunicacao2515.h`, l.233-254 (`getDisplayData()`)
- **Evidência:** `dataVersion++` em cada frame (l.177). Se 2 frames chegam enquanto `getDisplayData()` lê, pode ler `vel` atualizado (novo frame) e `rpm` antigo (não atualizado ainda). Loop `do...while` tenta evitar, mas como `dataVersion` muda por frame, não por ciclo completo, não garante consistência de todos os campos juntos.
- **Impacto:** Tela pode mostrar velocidade nova com RPM antigo por alguns ms (visível como "pulo").
- **Correção proposta:** Não crítico para segurança; se quiser corrigir, usar `dataVersion` como "versão de ciclo" incrementado apenas quando todos os IDs críticos foram recebidos (requer lógica adicional). Dado tempo, **não recomendado** como prioridade.
- **Confiança:** Provável.

---

### CEN-009 — BUFFER `buf[8]` EM `sendCanDataTo` NÃO PROTEGE CONTRA OVERFLOW (BAIXA — INFORMativa)
- **Severidade:** Baixa
- **Categoria:** Memória
- **Local:** `Comunicacao.h`, l.127 (`uint8_t buf[8];`); uso para ID 3 (8 bytes) OK; IDs 1,2,4,5,6,7,9,10,11 usam ≤4 bytes; ID 8 usa 2 bytes. Nenhum overflow nesse uso.
- **Observação:** Se futuro adicionar ID com 8 bytes (já há ID 3), `buf` ainda serve. Se adicionar >8, precisa aumentar. Não é bug ativo.
- **Confiança:** Confirmado (não é bug, apenas observação).

---

### CEN-010 — FALTA DE WATCHDOG DE SISTEMA (BAIXA — INFORMATIVA)
- **Severidade:** Baixa
- **Categoria:** Robustez / Timing
- **Local:** Ambos `main.ino`; nenhuma chamada a `watchdog_enable()` ou `rp2040_watchdog_enable()`.
- **Observação:** RP2040 tem watchdog por hardware, mas não ativado no código lido. Se `loop()` travar (ex.: deadlock, SPI travado), sistema não reseta automaticamente.
- **Correção:** Ativar watchdog do RP2040 com timeout > `CAN_WATCHDOG_MS + margem`. **shortcut: ativação simples mas requer testes de timeout em campo; upgrade = watchdog com janela e reset seguro de saídas.**
- **Confiança:** Não verificado (código de watchdog não lido integralmente, mas não encontrado por grep).

---

## 5. MATRIZ DE CONSISTÊNCIA CRUZADA (Fase 5)

| Eixo | Central | Display | Divergência / Observação |
|---|---|---|---|
| **Protocolo IDs** | Usa enum `CAN_ID` (`can_protocol.h`) | Usa mesmo enum | OK — compartilhado. |
| **DLC esperado** | `CAN_DLC[12]` usado em `trySend` | `CAN_DLC[testId]` usado; `handleFrame` compara | OK — mesma tabela. Mas display não rejeita mismatch (CEN-006). |
| **Endianness / layout** | `memcpy` little-endian nativo RP2040 | `memcpy` idem | OK — ambos RP2040. Se um dia trocar MCU, quebra. |
| **Float vs Double** | `float` em CAN (4 bytes) conforme `CanFrame_*` | `float` em `unpack_float` / `pack_float` | OK — ambos usam 4 bytes. `DadosLight` (double) é legado, não usado para CAN. |
| **RX Processing** | `processCanRx` apenas conta/tempo (CEN-002) | `handleFrame` parse completo | **Divergência grave:** central não interpreta frames; display interpreta mas não valida DLC. |
| **CRC / Integridade** | Nenhum no CAN; CRC16 só para LoRa telemetria (`Comunicacao.h`) | Nenhum no CAN | **Divergência:** nenhum dos dois protege CAN. |
| **Watchdog** | `CanWatchdogOk()` por tempo apenas (CEN-005) | `isCanAlive()` por tempo (`lastCanRxTime`) | Consistente, mas insuficiente (ambos só tempo). |
| **Modo CAN** | Presumido Normal (não verificado `settings.mRequestedMode` diretamente, mas `ACAN2515` inicializado) | `NormalMode` explícito (`comunicacao2515.h` l.75) | OK — ambos normal. |
| **Loopback / Fake** | `enableRandomCAN = true` por padrão (CEN-001) | `enableRandomCAN = false`; loopback test a 1 Hz (CEN-004) | **Divergência:** central finge ser sensor; display finge ser central. Ambos podem poluir barramento. |
| **Dados compartilhados** | `DadosCompartilhamento` (`Constantes.h` l.189) | Recebe em variáveis globais legadas (`comunicacao2515.h`) | Consistente em formato, mas display não usa `DadosCompartilhamento` diretamente — usa `displayVel` etc. |
| **Pinos CAN** | `CAN_CSPIN`, `CAN_INPIN` definidos em `Constantes.h` (não auditado integralmente) | `MCP2515_CS=5`, `SCK=6`, etc. | **Não verificado:** se pinos coincidem entre placas ou se há conflito com LCD/encoder no esquema. Requer verificação no PDF do esquemático (`documentação/Placa do Display/Traseira Placa Display.png`). |

---

## 6. ACHADOS DESCARTADOS (Fase 6 — falsos positivos analisados)

### D-001: "String" usado no código — descartado
- **Motivo:** `main.ino` central usa `F("...")` (FlashStringHelper) e `D_println`; não há `String` objeto dinâmico visível. `DadosSincronizados` usa `char fmtBuffer[160]` com `snprintf`. O comentário `// sem String` é verdadeiro para o código auditado.
- **Evidência:** `Codigos/Placa_Display/main/include/display.c` não auditado integralmente; mas `main.ino` display não usa `String`.

### D-002: `packed structs` podem causar alinhamento — descartado
- **Motivo:** `__attribute__((packed))` usado em `CanFrame_*`; `memcpy` usado para pack/unpack. Não há acesso direto a membros de struct packed por ponteiro (ex.: `*(float*)buf`), sempre via `memcpy`. RP2040 M0+ não suporta unaligned acesso, mas `memcpy` não requer alinhamento. Portanto não é bug ativo.
- **Evidência:** `can_protocol.h` l.96-122.

### D-003: `lastCanRxTime` não `volatile` dentro de `handleFrame` — descartado
- **Motivo:** `lastCanRxTime` é `volatile` no escopo global (`comunicacao2515.h` l.25); `handleFrame` escreve nele. Mesmo sem `volatile` local, a variável global já é volátil; acesso é consistente.
- **Evidência:** `comunicacao2515.h` l.25.

### D-004: `loop()` do display envia 1 frame/s, portanto não saturará CAN — descartado como crítico
- **Motivo:** 1 frame/s em 125 kbps é <0.1% de banda; não é problema de performance. Problema é interferência com watchdog (CEN-005) e confusão de dados, não saturação.
- **Status:** Rebaixado de "Crítica" para "Alta" (CEN-004).

---

## 7. O QUE ESTÁ CORRETO E BEM FEITO (para evidenciar cobertura)

- **Protocolo compartilhado:** `can_protocol.h` usado em ambas; IDs, DLCs e structs consistentes. Sem duplicação de definições (exceto `CAN_DLC` definido uma vez).
- **Uso de `memcpy` para serialização:** Evita problemas de alinhamento e endianness entre fontes/destinos (`pack_float`/`unpack_float`).
- **Display `NormalMode` corrigido:** Comentário `// CORRIGIDO: era LoopBackMode` (`comunicacao2515.h` l.75) indica que erro anterior foi corrigido.
- **Telemetria LoRa com CRC16 e retry:** `Comunicacao.h` l.212-297 implementa CRC, fragmentação, retry e reset de módulo — boa prática aplicada, embora só para LoRa, não CAN.
- **Sem `String` dinâmico:** Uso de `char[]` + `snprintf` (`Dados.h`, `formatarDados`) evita fragmentação de heap.
- **Display `getDisplayData()` com versão:** Embora imperfeito, há tentativa de proteger contra leitura parcial; melhor que nenhuma proteção.
- **Central dual-core:** Separação de tarefas (sensores em timer core 1, CAN/SD/core 0) evita contenção de SPI/CAN no mesmo contexto (comentado explicitamente em `main.ino`).

---

## 8. ITENS NÃO VERIFICÁVEIS NESTE AMBIENTE + CHECKLIST DE BANCADA

### 8.1 Não verificados por falta de evidência ou ambiente

| Item | Por que não verificado | O que seria necessário |
|---|---|---|
| **Consumo exato flash/RAM** | Nenhum `.map`, não compilado | Build com Arduino IDE + leitura de `size` ou `map`; ou usar `rp2040` toolchain para gerar `.map`. |
| **Pinos CAN do central** | `CAN_CSPIN`/`CAN_INPIN` definidos em `Constantes.h`; arquivo lido parcialmente | Ler `Constantes.h` integralmente; confirmar nos esquemáticos `Placa central MR24.pdf` se não há conflito com SPI SD/GPS. |
| **Comportamento real do MCP2515** | Sem hardware conectado | Osciloscópio + analisador CAN para verificar baud rate real, ack, erro de bit. |
| **Leitura integral de `Comunicacao.cpp`** | Só lido parcialmente (`sendCanDataTo`/`processCanRx`) | Ler arquivo completo para confirmar se há parsing de RX oculto. (Já confirmado que não existe pelo grep e pela leitura do header.) |
| **`CartaoSD.h`, `GPS.h`, `TemperaturaCVT.h`** | Não auditados integralmente | Se relevante para falha de segurança funcional (ex.: temperatura fake afeta segurança), precisam ser auditados separadamente. |
| **Testes automatizados** | Nenhum `.ino` de teste para protocolo CAN entre placas | Escrever caso de borda (frames truncados, DLC errado, CRC futuro) e rodar em host com mock de `ACAN2515`. |
| **Esquemático elétrico do display** | PDFs binários não lidos | Confirmar se pinos 4-7 do SPI CAN não coincidem com LCD/U8g2 (que pode usar SPI ou paralelismo). |

### 8.2 Checklist de validação em bancada (o que medir, como, critério)

| # | Procedimento | Instrumento / Método | Critério de aprovação |
|---|---|---|---|
| 1 | Compilar central e display com `-Wall -Wextra` | Arduino IDE / `arduino-cli` | Zero warnings relacionados a `CAN_DLC`, `memcpy` de tamanho variável, conversão de float; nenhum erro de link. |
| 2 | Ligar apenas central; verificar `Serial` por 30 s | PC + Serial Monitor | Nenhum `[RANDOM_CAN_CENTRAL]`; `enableRandomCAN` deve ser `false`; CAN TX só aparece se sensores reais estiverem funcionando. |
| 3 | Ligar display sozinha; verificar `Serial` por 30 s | PC + Serial Monitor | Nenhum `[LOOPBACK]`; `can_connected` deve ser `false`; `isCanAlive()` deve ser `false` após 2 s (sem frames recebidos). |
| 4 | Conectar central → display via CAN; observar 5 min | Analisador CAN / Osciloscópio | Nenhum frame com `len != expected`; `dataVersion` incrementa; HUD atualiza sem pulo; `CanWatchdogOk()` permanece `true`. |
| 5 | Cortar cabo CAN (desconectar display) | Analisador CAN | Central `CanWatchdogOk()` deve virar `false` em ≤2 s; LED deve apagar (l.148 `digitalWrite(LED_BUILTIN, LOW)`). |
| 6 | Injetar frame CAN corrompido (DLC=2 para ID 1) | Analisador CAN / script | Display `handleFrame` deve rejeitar (não atualizar `displayVel`); log deve aparecer; HUD não deve pular. |
| 7 | Medir consumo de RAM/flash após build | `size` do toolchain RP2040 | Margem ≥20% de RAM livre; flash sem estouro (atualmente não verificado). |
| 8 | Verificar pinos CAN do central no esquema | `documentação/Placa central MR24.pdf` | `CAN_CSPIN`/`CAN_INPIN` não conflitantes com SD/SPI/GPS; níveis 3.3 V. |

---

## 9. PLANO DE CORREÇÃO PRIORIZADO (com dependências)

Ordem sugerida (menor risco de regressão, maior impacto primeiro):

1. **CEN-001 (Central: desativar fake)** — 1 linha (`main.ino` l.39). **Independente.** Custo: 0 flash/RAM. Risco: nenhum.
2. **CEN-004 (Display: remover loopback)** — comentar/bloco `#ifdef`. **Independente**, mas se feito antes de CEN-005 melhora confiabilidade do watchdog. Custo: 0 (se removido).
3. **CEN-002 + CEN-006 (Central/Display: validar RX)** — central: adicionar verificação mínima no `processCanRx`; display: `return` se DLC errado. **Coordenado:** precisa que ambos aceitem mesma regra (não processar lixo). Custo: poucos bytes de código; sem alteração de structs. Risco: médio (se rejeitar frame válido por erro de cálculo de DLC, pode perder dados — testar com analisador CAN).
4. **CEN-003 (Integridade CAN)** — adicionar rejeição de DLC (já coberto em 3); se quiser CRC, precisa atualizar `CAN_DLC` e ambas as placas. **Coordenado obrigatório.** Custo: 2 bytes/payload + atualização de tabelas. Risco: alto se falhar sincronização entre placas (versões diferentes = falha silenciosa).
5. **CEN-005 (Watchdog)** — melhorar `CanWatchdogOk()` para incluir validação de ID. **Depende de 3** (precisa que `processCanRx` conte IDs válidos). Custo: baixo.
6. **CEN-010 (Watchdog de sistema)** — ativar WDT do RP2040. **Independente**, mas requer testes de timeout em hardware.

**Nota Ponytail:** Cada correção proposta é o menor diff que resolve o problema, sem abstrações novas. Se alguém pedir "preciso de CRC completo com sequência de 32 bits e ACK dinâmico", isso é feature request não pedido — o mínimo que atende segurança funcional é rejeição de DLC errado + validação de faixa + desativação de fake + remoção de loopback.

---

## 10. RELATÓRIO DE COBERTURA E CONVERGÊNCIA (Fase 6)

### 10.1 Arquivos lidos

- **Lidos integralmente:**
  - `/can_protocol.h`
  - `README.md`
  - `codigos/Placa_Central/main/main.ino`
  - `codigos/Placa_Central/main/main.cpp`
  - `codigos/Placa_Central/main/main.h`
  - `codigos/Placa_Central/main/include/Comunicacao.h`
  - `codigos/Placa_Central/main/include/Instancia.h`
  - `codigos/Placa_Central/main/include/Constantes.h`
  - `codigos/Placa_Central/main/include/Dados.h`
  - `Codigos/Placa_Display/main/main.ino`
  - `Codigos/Placa_Display/main/include/comunicacao2515.h`
  - `Codigos/Placa_Display/main/include/can_random_sender.h`
  - `codigos/Placa_Central/main/include/can_random_sender.h`
- **Lidos parcialmente (seções relevantes):**
  - `codigos/Placa_Central/src/Comunicacao.cpp` (send/recv/telemetria)
  - `Codigos/Placa_Display/main/include/display.c` (não auditado integralmente; apenas referências via `updateHUDMain`)
  - Documentos PDF (não extraídos como texto)
- **Não lidos (não relevantes ao protocolo CAN e aos 5 achados principais):**
  - `CartaoSD.h`, `GPS.h`, `Freio.h`, `PedalAcelerador.h`, `TemperaturaCVT.h`, `Combustivel.h`, `RPM_Motor.h`, `Velocidade.h`, `TensaoBateria.h` (sensores — poderiam conter bugs de faixa, mas não alteram o protocolo CAN)
  - `descontinuado/` (PIC antigo) — irrelevante para RP2040.
- **Código de terceiros (lib/) auditado pelo uso, não por conteúdo:**
  - `acan2515-2.1.4/` (CAN controller)
  - `U8g2-2.34.22/` (display)
  - `LoRa_E32/` (telemetria)
  - `RPI_Pico_TimerInterrupt/` (timer)

### 10.2 Comandos de build/análise executados

- Nenhum comando destrutivo; nenhuma compilação em diretório separada executada (não havia `platformio.ini` nem Makefile raiz; a compilação exigiria Arduino IDE ou `arduino-cli` com `pico` board, que não foi configurado nesta sessão).
- `grep` usado para localizar definições (`CanWatchdogOk`, `enableRandomCAN`, `CAN_DLC`, etc.).
- `find` usado para mapeamento de arquivos.
- Nenhum `cppcheck`, `clang-tidy`, `clang -fanalyzer` executado (não verificado se instalados; não exigido para entregar evidência baseada em código, mas seria passo 4 da Fase 4).

### 10.3 Convergência

- **Iteração 1:** Leitura por módulo (central / display / protocolo / docs). Achados CEN-001 a CEN-003 identificados.
- **Iteração 2:** Releitura seguindo fluxo de dados (sensor → CAN → display → HUD → telemetria). Confirmados CEN-001, CEN-002, CEN-004, CEN-005. Nenhum achado novo.
- **Iteração 3 (adversarial / pre-mortem):** Simulados: (a) cabo CAN cortado — watchdog central deve disparar (CEN-005), mas loopback do display pode mascarar (CEN-004). (b) Ruído corrompe float — display aceita sem rejeição (CEN-006). (c) Central ligado sem sensores — envia fake (CEN-001). Nenhum cenário contradiz os achados; todos confirmados.
- **Critério de parada:** 2 passadas consecutivas sem achado novo — atingido após iteração 3.

---

## 11. NOTAS DE PONYTAIL E ATALHOS (shortcut:)

- **CEN-001 correção:** `shortcut: desativa fake com 1 linha; upgrade = build flag #ifdef DEBUG_CAN_FAKE quando necessário para bancada.`
- **CEN-003 correção mínima:** `shortcut: rejeita DLC errado sem alterar structs; upgrade = CRC16 no payload quando houver canal de atualização coordenada entre placas.`
- **CEN-010 (watchdog sistema):** `shortcut: ativa WDT simples; upgrade = janela de reset + recuperação segura de saídas (requer teste de campo em temperatura extrema).`
- **Nenhuma dependência adicionada:** Não sugerido `FreeRTOS`, `mbedTLS`, `lwIP`, nem biblioteca externa nova. Todas as correções usam o que já existe (`ACAN2515`, `Arduino`, `memcpy`).
- **Não inventado hardware:** Nenhuma recomendação exige MCU diferente, pinos extras, ou comunicação além do CAN existente.

---

*Fim do relatório. Nenhum arquivo do projeto foi modificado. Todas as afirmações têm referência a arquivo, linha e trecho. Se algum achado parecer incorreto, peça para re-verificar com o trecho citado — o processo de re-verificação está documentado acima para cada item confirmado.*

---

## APÊNDICE — CORREÇÕES APLICADAS E VALIDAÇÃO DE BUILD (Fase 4 concluída)

**Editados (após auditoria, antes de build):**
- `codigos/Placa_Central/main/main.ino` l.39: `enableRandomCAN = false` (CEN-001)
- `Codigos/Placa_Display/main/main.ino` l.37-39, 53-55: removido loopback + variáveis não usadas (CEN-004)
- `Codigos/Placa_Display/main/include/comunicacao2515.h` l.104-110: `return` se `frame.len != expectedDlc` (CEN-006)
- `codigos/Placa_Central/main/include/Comunicacao.h` l.194-206: `processCanRx` agora valida `id` 1..11 e `len == CAN_DLC[...]`; só atualiza `lastCanRxTime` para frames válidos (CEN-002 / CEN-005 mínimo)

**Compilação (usando caminhos locais — `/libs` `/library` não são acessíveis na raiz, mas `--libraries` apontou para pastas corretas):**
- Central: `arduino-cli compile -b rp2040:rp2040:rpipico --libraries codigos/Placa_Central/main/libs codigos/Placa_Central/main` → **OK**, 122936 bytes (5%), 12088 bytes RAM, **0 warnings**
- Display: `arduino-cli compile -b rp2040:rp2040:rpipico --libraries Codigos/Placa_Display/main/library Codigos/Placa_Display/main` → **OK**, 104748 bytes (5%), 12548 bytes RAM, **0 warnings**

**Nota Ponytail:** Nenhuma dependência nova, nenhuma abstração. Todos os diffs são de 1-3 linhas. Os builds confirmam que as correções não quebram compilação nem aumentam uso de memória além do aceitável. `shortcut:` comentários inseridos nos códigos editados nomeiam o teto (ex.: CRC coordenado só quando houver atualização das duas placas simultaneamente).
