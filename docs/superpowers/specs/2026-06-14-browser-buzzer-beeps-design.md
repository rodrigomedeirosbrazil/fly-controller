# Espelhar beeps do buzzer no navegador

**Data:** 2026-06-14
**Status:** Aprovado (design)

## Objetivo

Reproduzir no navegador, na tela de telemetria do web portal, os mesmos beeps
emitidos pelo buzzer físico do controller. Os sons devem ser disparados
exatamente iguais (pitch, ritmo on/off e número de repetições) ao firmware.

Caso de uso: o piloto mantém a página de telemetria aberta no celular durante o
voo e recebe os mesmos alertas sonoros que o buzzer do controller produz —
sobretudo o alerta contínuo de "armado".

## Decisões de design

- **Transporte: piggyback no `/api/telemetry` (poll de 1s).** Latência de até 1s
  é aceitável. Custo desprezível (alguns inteiros num JSON já servido), sem
  conexão persistente, sem endpoint novo, sem lógica de reconexão. SSE foi
  descartado: sua única vantagem (latência) é neutralizada pelo requisito de 1s,
  enquanto seu custo (socket TCP persistente num rádio compartilhado com
  ESP-NOW/BLE) é real.
- **Captura dentro do `Buzzer` (fonte única de verdade).** Todos os beeps
  contextuais passam por `startBeep()`, então o ponto de captura é o próprio
  `Buzzer`. Zero mudança nos call sites (Throttle, Button, main, WebServer). O
  navegador espelha o que o buzzer realmente está tocando, não re-deriva de
  estado da aplicação.
- **Volume sempre no máximo.** O navegador emite sempre em ganho máximo quando
  desmutado; a intensidade real é controlada pelo volume do dispositivo
  (SO/hardware). Não há controle de volume na UI — apenas mute/unmute. O
  duty-cycle configurável do buzzer físico **não** é espelhado.
- **Áudio começa mudo.** A autoplay policy dos navegadores exige um gesto do
  usuário para iniciar o `AudioContext`. Um botão de som dedicado destrava o
  áudio no primeiro toque e serve de mute/unmute.

## Arquitetura

### 1. Captura no `Buzzer`

`Buzzer` passa a expor um snapshot público do que está tocando:

```cpp
struct BeepEvent {
  uint32_t seq;       // contador monotônico, +1 a cada startBeep()
  uint16_t frequency; // Hz
  uint16_t onMs;      // beepDuration
  uint16_t offMs;     // pauseDuration
  uint8_t  reps;      // 255 = contínuo
  bool     active;    // o buzzer está tocando agora?
};

BeepEvent getBeepEvent() const;
```

- `startBeep()` preenche o descritor (`frequency`, `onMs`, `offMs`, `reps`) e
  incrementa `seq`.
- `stop()` e o término natural do padrão setam `active = false`, preservando o
  último `seq`.
- `frequency` reflete a frequência efetiva do beep (resolve o caso
  `frequency == 0`, que usa o default `pwmFrequency`), para que o navegador toque
  o pitch correto.
- Melodias (`startMelody`) estão **fora de escopo** — nenhum beep contextual usa
  melodia. `getBeepEvent()` só descreve padrões simples de `startBeep()`.

### 2. Transporte — campo no `/api/telemetry`

O handler de `/api/telemetry` (em `ControllerWebServer.cpp`) adiciona um
sub-objeto a partir de `buzzer.getBeepEvent()`:

```json
"buzzer": { "seq": 42, "freq": 2300, "onMs": 150, "offMs": 80, "reps": 3, "active": true }
```

Sem mudanças no cadência do poll (mantém 1s).

### 3. Player no navegador (`TelemetryPage.h` → `telemetry.js`)

**Web Audio:** um `AudioContext` único; cada nota é um `OscillatorNode`
(`type = 'square'`, mais próximo do timbre do piezo) ligado a um `GainNode`
mestre (mute = 0, ativo = 1.0). Função genérica que espelha `startBeep`:

```
playPattern(freq, onMs, offMs, reps)  // agenda on/off via AudioContext.currentTime
```

**Lógica de decisão por poll** (compara o `buzzer` recebido com o último estado
visto — idealmente extraída numa função pura `nextAction(prev, cur)` para teste):

- `seq` avançou e `reps < 255` (one-shot) → toca o padrão **uma vez**.
- `seq` avançou e `reps == 255` (contínuo) → inicia **loop local** do padrão
  (alerta de armado). Cada dispositivo roda seu próprio timing 200/200; não há
  alinhamento de fase, mas o padrão é idêntico.
- buzzer reporta `active == false` enquanto um loop está rodando → **para** o
  loop.

**Trade-off aceito:** se dois one-shots distintos caírem na mesma janela de 1s,
apenas o último toca (o `seq` indica que houve beeps, mas só o descritor mais
recente está disponível).

### 4. UI — botão de som dedicado

- Ícone 🔔/🔇 na `.telemetry-status-bar`, espelhando o padrão do botão de
  cadeado (wake-lock) já existente.
- Primeiro toque: `audioContext.resume()` (destrava a autoplay policy) +
  desmuta.
- Toggle mute/unmute; estado persistido em `localStorage`. Começa **mudo**.
- Mute apenas zera o ganho mestre; o rastreamento de `seq`/loop continua, então
  desmutar **não** dispara beeps antigos acumulados — só passa a tocar os
  próximos.

### 5. Fidelidade

Pitch, ritmo (on/off) e repetições idênticos ao firmware. Timbre aproximado por
onda quadrada (piezo passivo ≈ square). Volume absoluto é do dispositivo.

## Fora de escopo

- Melodias (`startMelody`).
- Espelhar o volume/duty-cycle configurável do buzzer.
- Beeps do remote (já cobertos pelo ESP-NOW/`RemoteLink`).
- Latência sub-1s / SSE.

## Testes

- A lógica de buzzer usa LEDC (hardware), então o `BeepEvent` é validado
  principalmente por inspeção: struct simples preenchido em `startBeep()`/`stop()`.
- A transição de estado do player é a parte com risco de regressão. Extrair
  `nextAction(prev, cur)` como função pura em JS permite um teste leve
  (one-shot dispara, contínuo inicia loop, `active=false` para o loop, sem
  re-disparo ao desmutar).
- Validação manual no navegador: armar (alerta contínuo), clicar botão
  (one-shot), calibrar, etc., confirmando que o som no navegador casa com o
  buzzer físico.
