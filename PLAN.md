# Arquitetura de Firmware — Controlador de Motores Elétricos

**Plataforma:** ESP32-C3 Supermini  
**Framework:** Arduino / PlatformIO  
**Fabricantes suportados:** Hobbywing, T-Motor, XAG  
**Status:** Documento de planejamento — não implementado

---

## Índice

1. [Contexto e motivação](#1-contexto-e-motivação)
2. [Diagnóstico do código atual](#2-diagnóstico-do-código-atual)
3. [Princípios da nova arquitetura](#3-princípios-da-nova-arquitetura)
4. [Estrutura de pastas](#4-estrutura-de-pastas)
5. [Descrição de cada componente](#5-descrição-de-cada-componente)
6. [Fluxo de dados](#6-fluxo-de-dados)
7. [Regras arquiteturais](#7-regras-arquiteturais)
8. [Fontes de dados por fabricante](#8-fontes-de-dados-por-fabricante)
9. [Decisões de design e justificativas](#9-decisões-de-design-e-justificativas)
10. [Plano de migração incremental](#10-plano-de-migração-incremental)
11. [Pontos em aberto](#11-pontos-em-aberto)

---

## 1. Contexto e motivação

O firmware controla motores elétricos de três fabricantes distintos: **Hobbywing**, **T-Motor** e **XAG**. Cada fabricante usa protocolos e fontes de dados diferentes:

| Fabricante | Motor Temp    | ESC Temp              | Tensão Bateria        | Corrente | RPM | CAN |
|------------|---------------|-----------------------|-----------------------|----------|-----|-----|
| Hobbywing  | ADS1115       | CAN                   | CAN                   | CAN      | CAN | Sim |
| T-Motor    | ADS1115       | CAN                   | CAN                   | CAN      | CAN | Sim |
| XAG        | ADC → ADS1115 | ADC → ADS1115         | ADC → ADS1115         | —        | —   | Não |

> A coluna XAG usa ADC built-in hoje e migrará para ADS1115 em breve. A arquitetura proposta já prevê essa transição.

Um firmware separado é compilado para cada fabricante via `platformio.ini`:

```ini
[env:lolin_c3_mini_hobbywing]
build_flags = -D CONTROLLER_TYPE=2

[env:lolin_c3_mini_tmotor]
build_flags = -D CONTROLLER_TYPE=3

[env:lolin_c3_mini_xag]
build_flags = -D CONTROLLER_TYPE=1
```

O código atual apresenta problemas de organização que dificultam manutenção e evolução. Este documento descreve a arquitetura proposta para resolver esses problemas.

---

## 2. Diagnóstico do código atual

### 2.1 Estrutura atual problemática — `Telemetry/`

O diretório atual `Telemetry/` usa um padrão de `TelemetryProvider` baseado em ponteiros de função dentro de um struct:

```cpp
struct TelemetryProvider {
    void (*update)(void);
    void (*init)(void);
    TelemetryData* (*getData)(void);
    void (*sendNodeStatus)(void);
    void (*handleCanMessage)(twai_message_t*);
    bool (*hasTelemetry)(void);
    TelemetryData data;
};
```

Cada fabricante tem um `create*Provider()` que preenche esse struct com funções estáticas locais.

### 2.2 Problemas identificados

**Mistura de responsabilidades**  
A camada `Telemetry` atual acumula: leitura de hardware, parse de CAN, regra de negócio, armazenamento de estado e envio de NodeStatus. Isso viola o princípio de responsabilidade única.

**`#if` espalhado por múltiplos arquivos**  
Diretivas de compilação condicional aparecem dentro de `Temperature.cpp`, `Throttle.cpp`, `Canbus.cpp`, `Power.cpp`, `BatteryMonitor.cpp`, `Xctod.cpp` e `main.cpp`. Cada novo fabricante exige modificações em vários arquivos.

**`Temperature` acoplada ao firmware**  
A classe `Temperature` tem dentro de si:
```cpp
#if IS_TMOTOR || IS_HOBBYWING
    oversampledValue = ads1115.readChannel(ADS1115_TEMP_CHANNEL);
#else
    oversampledValue += analogRead(pin);
#endif
```
Uma classe de sensor não deve saber em qual firmware está rodando.

**`Throttle` acoplada ao firmware**  
O mesmo problema ocorre em `Throttle.cpp`:
```cpp
#if IS_TMOTOR || IS_HOBBYWING
    raw = ads1115.readChannel(ADS1115_THROTTLE_CHANNEL);
#else
    raw = analogRead(THROTTLE_PIN);
#endif
```
A classe de throttle também não deve saber em qual firmware está. O padrão `ReadFn` resolve isso da mesma forma que em `Temperature`.

**`Canbus` conhece os drivers de fabricante**  
`Canbus::handle()` chama diretamente `hobbywing.handle()` e `tmotor.handle()`. `Canbus::parseCanMsg()` decide qual driver chamar baseado no `dataTypeId`. O barramento CAN não deve conhecer os protocolos de fabricante.

**Globals ocultos nos providers**  
Ponteiros estáticos como `g_hobbywingProvider` apontam para variáveis estáticas locais de funções. Isso cria armadilhas de ciclo de vida difíceis de debugar.

**Escrita manual de temperatura no `main.cpp`**  
```cpp
// No loop():
TelemetryData* data = telemetry->getData();
data->motorTemperatureMilliCelsius = (int32_t)(motorTempCelsius * 1000.0f);
```
Lógica de negócio vazando para o loop principal.

**Funções de formatação misturadas com struct de dados**  
`TelemetryData.h` contém as funções `formatVoltage()` e `formatTemperature()` inline, misturando dados passivos com lógica de apresentação.

**Logging excessivo em produção**  
Os drivers CAN (`Hobbywing.cpp`, `Tmotor.cpp`) contêm diversas chamadas `Serial.println()` que ficam ativas em produção, consumindo tempo de CPU desnecessariamente e dificultando a distinção entre logs de diagnóstico e logs de erro real.

---

## 3. Princípios da nova arquitetura

**Responsabilidade única por classe**  
Cada classe tem uma razão clara para existir e uma razão clara para mudar.

**`#if` concentrado**  
Diretivas de compilação condicional ficam apenas em `config.cpp` (instanciação) e em `Telemetry.cpp` (seleção de implementação). Nunca dentro de classes de sensor, CAN ou lógica de negócio.

**Fabricante como módulo auto-contido**  
Tudo que é específico de um fabricante vive dentro da pasta do fabricante. Adicionar um novo fabricante não exige modificar código existente.

**Fachada estável para consumidores**  
A classe `Telemetry` é o único ponto de acesso a dados para todos os consumidores. Mudanças internas de implementação não afetam `Power`, `BatteryMonitor`, `Xctod` ou `WebServer`.

**Sensores agnósticos de firmware**  
Classes como `Temperature`, `Throttle` e `BatteryVoltageSensor` não sabem se estão num Hobbywing ou XAG. Recebem uma função de leitura ADC pelo construtor.

**Pull, não push**  
`Telemetry::update()` chama ativamente `*Telemetry::update()` e lê os dados. O fluxo de dados segue a chamada, tornando o comportamento previsível e fácil de debugar.

---

## 4. Estrutura de pastas

```
src/
│
├── Hobbywing/
│   ├── HobbywingCan.h          ← protocolo DroneCAN: parse de frames + envio de comandos
│   ├── HobbywingCan.cpp
│   ├── HobbywingTelemetry.h    ← agrega: HobbywingCan (CAN) + motorTemp (ADS1115)
│   └── HobbywingTelemetry.cpp
│
├── Tmotor/
│   ├── TmotorCan.h             ← protocolo TM-UAVCAN v2.3: parse + envio
│   ├── TmotorCan.cpp
│   ├── TmotorTelemetry.h       ← agrega: TmotorCan (CAN) + motorTemp (ADS1115)
│   └── TmotorTelemetry.cpp
│
├── Xag/
│   ├── XagTelemetry.h          ← agrega: motorTemp + escTemp + batterySensor (ADC/ADS1115)
│   └── XagTelemetry.cpp
│
├── Telemetry/
│   ├── Telemetry.h             ← fachada pública com getters estáveis
│   ├── Telemetry.cpp           ← #if seleciona implementação de fabricante
│   ├── TelemetryData.h         ← struct de dados puro, sem lógica
│   └── TelemetryFormatter.h    ← funções inline: voltage/temp → string (sem .cpp)
│
├── Canbus/
│   ├── Canbus.h                ← protocolo DroneCAN genérico (agnóstico de fabricante)
│   ├── Canbus.cpp              ← receive(), sendNodeStatus(), GetNodeInfo
│   └── CanUtils.h              ← utilitários de frame CAN (sem mudança)
│
├── Sensors/
│   ├── BatteryVoltageSensor.h  ← conversão ADC → millivolts com ReadFn
│   └── BatteryVoltageSensor.cpp
│
├── Temperature/
│   ├── Temperature.h           ← NTC via Steinhart-Hart, sem #if interno
│   └── Temperature.cpp
│
├── Throttle/
│   ├── Throttle.h              ← leitura de throttle, sem #if interno
│   └── Throttle.cpp
│
├── ADS1115/                    ← sem mudança
├── Power/                      ← consome Telemetry (fachada)
├── BatteryMonitor/             ← consome Telemetry (fachada)
├── Xctod/                      ← consome Telemetry (fachada)
├── WebServer/                  ← consome Telemetry (fachada), usa TelemetryFormatter
├── Buzzer/                     ← sem mudança
├── Button/                     ← sem mudança
├── Settings/                   ← sem mudança
├── Logger/                     ← sem mudança
│
├── config.h                    ← externs globais, defines de pinos
├── config.cpp                  ← instanciação de todos os objetos; lambdas ReadFn por firmware
├── config_controller.h         ← IS_XAG / IS_HOBBYWING / IS_TMOTOR
└── main.cpp                    ← setup() e loop() limpos
```

---

## 5. Descrição de cada componente

### 5.1 `HobbywingCan` / `TmotorCan`

**O que é:** A classe de comunicação CAN do fabricante. É a única que interpreta `twai_message_t` dentro do contexto do fabricante.

**Responsabilidades:**
- Fazer parse de todos os frames CAN recebidos do ESC
- Armazenar o estado interno extraído (RPM, tensão, corrente, ESC temp, ESC ID)
- Enviar comandos: throttle, configuração de LED, direção, request de ESC ID
- Gerenciar o `transferId` e a cadência de 400 Hz de throttle via `handle()`

**Não é responsabilidade:**
- Montar `TelemetryData`
- Saber o que é temperatura do motor — essa vem do ADS1115, não do CAN
- Chamar `Canbus` diretamente — recebe o frame já extraído pelo `main.cpp`

**Interface pública esperada (Hobbywing como exemplo):**
```cpp
class HobbywingCan {
public:
    // Chamado pelo main.cpp quando chega um frame de dados
    void parse(twai_message_t* msg);

    // Cadência periódica: envia throttle a 400 Hz
    void handle();

    // Comandos ao ESC
    void requestEscId();
    void setThrottleSource(uint8_t source);
    void setLedColor(uint8_t color, uint8_t blink = 0);
    void setRawThrottle(int16_t throttle);

    // Estado interno — lido por HobbywingTelemetry
    uint16_t getRpm()                     const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps()  const;
    uint8_t  getEscTemperature()           const;  // ← ESC temp via CAN
    bool     hasTelemetry()                const;
    bool     isReady()                     const;
};
```

**Diferença para o código atual:**  
A classe `Hobbywing` atual já faz parse e send corretamente. A mudança é principalmente de nome (`Hobbywing` → `HobbywingCan`) e remoção de qualquer referência a `TelemetryData` ou `Canbus` de dentro dela.

**Logging:** Substituir `Serial.println()` internos por `DEBUG_PRINTLN()` (ver seção 5.9). Mensagens de erro crítico podem permanecer como `Serial.println()`.

---

### 5.2 `HobbywingTelemetry` / `TmotorTelemetry`

**O que é:** O agregador de telemetria do fabricante CAN. Sabe de onde vem cada dado e monta um estado coerente.

**Responsabilidades:**
- Ler dados CAN via getters do `HobbywingCan` / `TmotorCan`
- Ler temperatura do motor via objeto `Temperature` (ADS1115)
- Manter estado interno atualizado e expor getters

**Não é responsabilidade:**
- Fazer parse de CAN
- Conhecer detalhes do protocolo de comunicação

**Origem explícita de cada dado:**

| Dado              | Origem     | Como é obtido                             |
|-------------------|------------|-------------------------------------------|
| Tensão bateria    | CAN        | `hobbywingCan.getBatteryVoltageMilliVolts()` |
| Corrente bateria  | CAN        | `hobbywingCan.getBatteryCurrentMilliAmps()`  |
| RPM               | CAN        | `hobbywingCan.getRpm()`                   |
| **Temperatura ESC**   | **CAN**    | **`hobbywingCan.getEscTemperature()`**    |
| **Temperatura motor** | **ADS1115**| **`motorTemp.getTemperature()`**          |

> **Importante:** Para Hobbywing e T-Motor **não existe** um objeto `Temperature escTemp`. A temperatura do ESC é medida pelo próprio ESC e enviada via CAN — é um dado do protocolo, não de um sensor local. O objeto `Temperature escTemp` só existe no firmware XAG.

**Acesso a dependências** via `extern` global:
```cpp
// HobbywingTelemetry.cpp
extern HobbywingCan hobbywingCan;
extern Temperature  motorTemp;
// Não há "extern Temperature escTemp" aqui — ESC temp vem do CAN
```

**Interface pública esperada:**
```cpp
class HobbywingTelemetry {
public:
    void update();

    bool          hasData()                        const;
    uint16_t      getBatteryVoltageMilliVolts()    const;
    uint32_t      getBatteryCurrentMilliAmps()     const;
    uint16_t      getRpm()                         const;
    int32_t       getMotorTempMilliCelsius()       const;  // ← ADS1115
    int32_t       getEscTempMilliCelsius()         const;  // ← CAN
    unsigned long getLastUpdate()                  const;
};
```

---

### 5.3 `XagTelemetry`

**O que é:** O agregador de telemetria do XAG. Sem CAN, todas as fontes são locais (ADC ou ADS1115).

**Responsabilidades:**
- Ler temperatura do motor via `Temperature`
- Ler temperatura do ESC via `Temperature`
- Ler tensão da bateria via `BatteryVoltageSensor`
- Expor os dados via getters

**Origem explícita de cada dado:**

| Dado              | Origem           | Como é obtido                        |
|-------------------|------------------|--------------------------------------|
| Tensão bateria    | ADC / ADS1115    | `batterySensor.getVoltageMilliVolts()` |
| Corrente bateria  | —                | zero (não disponível)                |
| RPM               | —                | zero (não disponível)                |
| **Temperatura ESC**   | **ADC / ADS1115**| **`escTemp.getTemperature()`**       |
| **Temperatura motor** | **ADC / ADS1115**| **`motorTemp.getTemperature()`**     |

> **Contraste com Hobbywing/Tmotor:** No XAG existe `Temperature escTemp` porque não há CAN para receber esse dado — ele precisa de um sensor físico local.

**Acesso a dependências:**
```cpp
// XagTelemetry.cpp
extern Temperature          motorTemp;
extern Temperature          escTemp;          // ← exclusivo do XAG
extern BatteryVoltageSensor batterySensor;
```

**Nota sobre migração para ADS1115:**  
`XagTelemetry` não precisa mudar quando o XAG receber ADS1115. Apenas as lambdas injetadas em `config.cpp` mudam (ver seção 5.6 e 5.7).

---

### 5.4 `Telemetry` (fachada)

**O que é:** O único ponto de acesso a dados de telemetria para todos os consumidores do sistema.

**Responsabilidades:**
- Expor getters limpos e estáveis
- Internamente delegar para `HobbywingTelemetry`, `TmotorTelemetry` ou `XagTelemetry` via `#if`
- Chamar `update()` do fabricante quando sua própria `update()` for chamada (pull)

**Não é responsabilidade:**
- Ler nenhum hardware diretamente
- Conhecer protocolos CAN
- Tomar decisões de negócio

**Implementação de `Telemetry.cpp`:**
```cpp
#include "Telemetry.h"
#include "../config_controller.h"

#if IS_HOBBYWING
    #include "../Hobbywing/HobbywingTelemetry.h"
    extern HobbywingTelemetry hobbywingTelemetry;
    #define IMPL hobbywingTelemetry
#elif IS_TMOTOR
    #include "../Tmotor/TmotorTelemetry.h"
    extern TmotorTelemetry tmotorTelemetry;
    #define IMPL tmotorTelemetry
#elif IS_XAG
    #include "../Xag/XagTelemetry.h"
    extern XagTelemetry xagTelemetry;
    #define IMPL xagTelemetry
#endif

void     Telemetry::update()                        { IMPL.update(); }
bool     Telemetry::hasData()                 const { return IMPL.hasData(); }
uint16_t Telemetry::getBatteryVoltageMilliVolts() const { return IMPL.getBatteryVoltageMilliVolts(); }
uint32_t Telemetry::getBatteryCurrentMilliAmps()  const { return IMPL.getBatteryCurrentMilliAmps(); }
uint16_t Telemetry::getRpm()                  const { return IMPL.getRpm(); }
int32_t  Telemetry::getMotorTempMilliCelsius() const { return IMPL.getMotorTempMilliCelsius(); }
int32_t  Telemetry::getEscTempMilliCelsius()  const { return IMPL.getEscTempMilliCelsius(); }
unsigned long Telemetry::getLastUpdate()      const { return IMPL.getLastUpdate(); }
```

**Interface pública:**
```cpp
class Telemetry {
public:
    void          init();
    void          update();

    bool          hasData()                        const;
    uint16_t      getBatteryVoltageMilliVolts()    const;
    uint32_t      getBatteryCurrentMilliAmps()     const;
    uint16_t      getRpm()                         const;
    int32_t       getMotorTempMilliCelsius()       const;
    int32_t       getEscTempMilliCelsius()         const;
    unsigned long getLastUpdate()                  const;
};

extern Telemetry telemetry;
```

---

### 5.5 `Canbus`

**O que é:** O driver de baixo nível do barramento CAN. Agnóstico de fabricante.

**Responsabilidades:**
- Receber frames brutos via `receive()` e devolver ao chamador
- Enviar `NodeStatus` periódico (protocolo DroneCAN genérico)
- Responder a `GetNodeInfo` (protocolo DroneCAN genérico)
- Detectar Node ID do ESC via `NodeStatus` recebido

**Não é responsabilidade:**
- Conhecer `HobbywingCan`, `TmotorCan` ou qualquer protocolo de fabricante
- Rotear mensagens para drivers específicos
- Chamar `handle()` de nenhum fabricante

**`receive()` — comportamento:**  
Processa internamente `NodeStatus` e `GetNodeInfo` (protocolo genérico DroneCAN). Para tudo mais, devolve `true` e preenche `outMsg` para o chamador processar.

```cpp
class Canbus {
public:
    bool    receive(twai_message_t* outMsg);  // non-blocking
    void    sendNodeStatus();
    void    requestNodeInfo(uint8_t targetNodeId);
    void    printCanMsg(twai_message_t* msg);
    uint8_t getNodeId()    const;
    uint8_t getEscNodeId() const;
};
```

**`main.cpp` processa o frame retornado:**
```cpp
void checkCanbus() {
    twai_message_t msg;
    if (canbus.receive(&msg)) {
#if IS_HOBBYWING
        hobbywingCan.parse(&msg);
#elif IS_TMOTOR
        tmotorCan.parse(&msg);
#endif
    }

    canbus.sendNodeStatus();

    // Cadência de throttle — cada driver gerencia o próprio timing
#if IS_HOBBYWING
    hobbywingCan.handle();
#elif IS_TMOTOR
    tmotorCan.handle();
#endif
}
```

> **Alerta:** O `Canbus::handle()` atual chama `hobbywing.handle()` e `tmotor.handle()` diretamente. Isso deve ser removido — o `handle()` de throttle pertence a cada driver de fabricante e deve ser chamado pelo `main.cpp`.

---

### 5.6 `Temperature`

**O que é:** Classe de leitura de sensor NTC via equação de Steinhart-Hart. Agnóstica de firmware.

**Mudança principal:**  
Remove o `#if IS_TMOTOR || IS_HOBBYWING` interno. Recebe uma função de leitura ADC no construtor.

```cpp
class Temperature {
public:
    typedef int (*ReadFn)();
    Temperature(ReadFn readFn);
    void   handle();
    double getTemperature() const;
};
```

**Instanciação em `config.cpp`:**
```cpp
// Hobbywing e Tmotor: ADS1115 canal 1
Temperature motorTemp([]() {
    return ads1115.readChannel(ADS1115_TEMP_CHANNEL);
});

// XAG atual: ADC built-in
Temperature motorTemp([]() { return analogRead(MOTOR_TEMPERATURE_PIN); });
Temperature escTemp  ([]() { return analogRead(ESC_TEMPERATURE_PIN); });

// XAG futuro: ADS1115
Temperature motorTemp([]() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); });
Temperature escTemp  ([]() { return ads1115.readChannel(ADS1115_ESC_TEMP_CHANNEL); });
```

A classe `Temperature` não precisa mudar para a migração do XAG para ADS1115 — apenas as lambdas em `config.cpp`.

---

### 5.7 `Throttle`

**O que é:** Classe de leitura do sinal de throttle (sensor Hall ou potenciômetro). Agnóstica de firmware.

**Mudança principal:**  
Remove o `#if IS_TMOTOR || IS_HOBBYWING` interno. Recebe uma função de leitura ADC no construtor, exatamente como `Temperature`.

```cpp
class Throttle {
public:
    typedef int (*ReadFn)();
    Throttle(ReadFn readFn);
    void         handle();
    unsigned int getThrottleRaw()        const;
    unsigned int getThrottlePercentage() const;
    bool         isArmed()               const;
    bool         isCalibrated()          const;
    // ... demais métodos existentes sem mudança
};
```

**Instanciação em `config.cpp`:**
```cpp
// Hobbywing e Tmotor: ADS1115 canal 0
Throttle throttle([]() {
    return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL);
});

// XAG: ADC built-in
Throttle throttle([]() {
    return analogRead(THROTTLE_PIN);
});
```

A lógica de calibração, deadband e mapeamento permanece inteiramente dentro da classe — apenas a origem da leitura bruta é injetada.

---

### 5.8 `BatteryVoltageSensor` (novo)

**O que é:** Classe dedicada à leitura de tensão da bateria via divisor resistivo. Agnóstica de firmware.

**Motivação:**  
A lógica de conversão ADC → millivolts (divisor resistivo, calibração, oversampling) está hoje inline no provider XAG. Extraindo para uma classe com `ReadFn`, a migração do XAG de ADC para ADS1115 se reduz a trocar a lambda em `config.cpp`.

```cpp
class BatteryVoltageSensor {
public:
    typedef int (*ReadFn)();
    BatteryVoltageSensor(ReadFn readFn, float dividerRatio);
    void     handle();
    uint16_t getVoltageMilliVolts() const;
};
```

**Instanciação em `config.cpp`:**
```cpp
// XAG atual: ADC built-in
BatteryVoltageSensor batterySensor(
    []() { return analogRead(BATTERY_VOLTAGE_PIN); },
    BATTERY_DIVIDER_RATIO
);

// XAG futuro: ADS1115
BatteryVoltageSensor batterySensor(
    []() { return ads1115.readChannel(ADS1115_BATTERY_CHANNEL); },
    BATTERY_DIVIDER_RATIO
);
```

---

### 5.9 `TelemetryData`

**O que é:** Struct puro de dados. Sem lógica, sem métodos, sem herança.

```cpp
struct TelemetryData {
    bool          hasTelemetry;
    uint16_t      batteryVoltageMilliVolts;
    uint32_t      batteryCurrentMilliAmps;
    uint16_t      rpm;
    int32_t       motorTemperatureMilliCelsius;
    int32_t       escTemperatureMilliCelsius;
    unsigned long lastUpdate;
};
```

As funções de formatação (`formatVoltage`, `formatTemperature`) que estavam inline neste header são movidas para `TelemetryFormatter.h`.

---

### 5.10 `TelemetryFormatter.h` (novo)

**O que é:** Header com funções `inline` de formatação de dados de telemetria para exibição textual.

**Motivação:**  
As funções de formatação existiam inline em `TelemetryData.h`, poluindo um struct que deve ser passivo. Ao mesmo tempo, não podem ficar duplicadas em cada consumidor (`WebServer`, `Xctod`, etc.). Um header dedicado com funções `inline` resolve os três problemas: `TelemetryData` fica passivo, não há duplicação, e não há overhead de chamada de função.

```cpp
// Telemetry/TelemetryFormatter.h
#pragma once
#include <Arduino.h>

// 44100 mV → "44.100"
inline String formatVoltage(uint16_t mV) {
    uint16_t volts    = mV / 1000;
    uint16_t decimals = mV % 1000;
    String result = String(volts) + ".";
    if (decimals < 10)       result += "00";
    else if (decimals < 100) result += "0";
    result += String(decimals);
    return result;
}

// 50000 m°C → "50"
inline String formatTemperature(int32_t mC) {
    return String(mC / 1000);
}

// 50123 m°C → "50.123"
inline String formatTemperatureWithDecimals(int32_t mC) {
    int32_t celsius  = mC / 1000;
    int32_t decimals = abs(mC % 1000);
    String result = String(celsius) + ".";
    if (decimals < 10)       result += "00";
    else if (decimals < 100) result += "0";
    result += String(decimals);
    return result;
}
```

Consumidores incluem `TelemetryFormatter.h` somente quando precisam formatar valores para exibição. Não há `.cpp` correspondente.

---

### 5.11 Debug logging (macro `DEBUG_PRINTLN`)

**Problema atual:**  
Os drivers CAN (`HobbywingCan`, `TmotorCan`) contêm diversas chamadas `Serial.println()` que ficam ativas em produção, consumindo tempo de execução desnecessariamente e dificultando a distinção entre mensagens de diagnóstico e erros reais.

**Solução:** Macro de debug em `config.h` que compila para zero em produção:

```cpp
// config.h
#ifdef DEBUG
    #define DEBUG_PRINT(x)   Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#endif
```

Ativação por ambiente em `platformio.ini`:
```ini
[env:lolin_c3_mini_hobbywing_debug]
build_flags =
    ${env.build_flags}
    -D CONTROLLER_TYPE=2
    -D DEBUG=1
```

**Critério de uso:**
- `DEBUG_PRINTLN()` — mensagens de diagnóstico e estado (parse de frames, detecção de ESC ID, etc.)
- `Serial.println()` — erros críticos que devem aparecer sempre em produção (falha ao instalar driver TWAI, timeout crítico, etc.)

---

### 5.12 `config.cpp`

**O que é:** O único lugar onde objetos globais são instanciados e onde o `#if` de fabricante aparece para definir quais objetos existem e com quais lambdas.

**Responsabilidade:**
- Instanciar todos os objetos globais
- Injetar as lambdas `ReadFn` corretas em `Temperature`, `Throttle` e `BatteryVoltageSensor` conforme o firmware
- Não conter lógica de negócio

---

## 6. Fluxo de dados

### 6.1 Hobbywing / T-Motor (com CAN)

```
┌─────────────────────────────────────────────────────┐
│                      CAN BUS                         │
└───────────────────────┬─────────────────────────────┘
                        │ frame recebido
                        ▼
                  Canbus::receive()
                  (trata NodeStatus e GetNodeInfo internamente)
                        │ frame de dados (true)
                        ▼
                   main.cpp / checkCanbus()
                        │
                        ▼
             HobbywingCan::parse(msg)
             (atualiza estado interno:
              RPM, tensão, corrente,
              ESC temp via CAN, ESC ID)

              [a cada loop]

             Telemetry::update()
                  │ pull
                  ▼
       HobbywingTelemetry::update()
          ├── hobbywingCan.getRpm()
          ├── hobbywingCan.getBatteryVoltageMilliVolts()
          ├── hobbywingCan.getBatteryCurrentMilliAmps()
          ├── hobbywingCan.getEscTemperature()    ← ESC temp via CAN
          └── motorTemp.getTemperature()           ← motor temp via ADS1115
                  │
                  ▼
┌─────────────────────────────────────────────────────┐
│              Consumidores                            │
│  Power / BatteryMonitor / Xctod / WebServer          │
│  → telemetry.getBatteryVoltageMilliVolts()           │
│  → telemetry.getEscTempMilliCelsius()                │
│  → telemetry.getRpm()  etc.                          │
└─────────────────────────────────────────────────────┘
```

### 6.2 XAG (sem CAN)

```
              [a cada loop]

  Temperature::handle()          ← motorTemp (ADC ou ADS1115)
  Temperature::handle()          ← escTemp   (ADC ou ADS1115)
  BatteryVoltageSensor::handle() ← batterySensor (ADC ou ADS1115)

             Telemetry::update()
                  │ pull
                  ▼
          XagTelemetry::update()
          ├── motorTemp.getTemperature()
          ├── escTemp.getTemperature()          ← sensor local (não CAN)
          └── batterySensor.getVoltageMilliVolts()
                  │
                  ▼
┌─────────────────────────────────────────────────────┐
│              Consumidores                            │
│  Power / BatteryMonitor / Xctod / WebServer          │
└─────────────────────────────────────────────────────┘
```

### 6.3 Envio de throttle (CAN)

```
  Throttle::handle()       ← lê ADS1115 ou ADC via ReadFn
       │
       ▼
  throttle.getThrottleRaw()
       │
       ▼
  Power::getPwm()
       │
  esc.writeMicroseconds()  ← sinal PWM físico para o ESC

  HobbywingCan::handle()   ← também envia throttle via CAN a 400 Hz
  (chamado em checkCanbus() no main.cpp)
```

---

## 7. Regras arquiteturais

Estas regras devem ser respeitadas durante a implementação. Qualquer violação indica que algo foi modelado no lugar errado.

| Regra | Descrição |
|-------|-----------|
| **R1** | `HobbywingCan` e `TmotorCan` são os únicos que interpretam `twai_message_t` dentro do contexto do fabricante |
| **R2** | `Canbus` não conhece `HobbywingCan`, `TmotorCan` ou qualquer driver de fabricante |
| **R3** | `*Telemetry` não faz parse de CAN — só lê getters |
| **R4** | `Telemetry` (fachada) é o único ponto de acesso a dados para `Power`, `BatteryMonitor`, `Xctod`, `WebServer` |
| **R5** | `Temperature`, `Throttle` e `BatteryVoltageSensor` não contêm `#if IS_*` |
| **R6** | `#if IS_HOBBYWING / IS_TMOTOR / IS_XAG` aparece apenas em `config.cpp`, `Telemetry.cpp` e `main.cpp` |
| **R7** | `Canbus::handle()` não chama `handle()` de nenhum driver de fabricante |
| **R8** | Nenhuma classe de fabricante escreve diretamente em `TelemetryData` — apenas expõe getters |
| **R9** | `Temperature escTemp` só existe no firmware XAG — Hobbywing e Tmotor recebem ESC temp via CAN |
| **R10** | Formatação de dados para exibição usa `TelemetryFormatter.h` — nunca inline nos consumidores |

---

## 8. Fontes de dados por fabricante

### 8.1 Origem física de cada dado

| Dado              | Hobbywing      | T-Motor        | XAG (atual)    | XAG (futuro)   |
|-------------------|----------------|----------------|----------------|----------------|
| Tensão bateria    | CAN            | CAN            | ADC built-in   | ADS1115        |
| Corrente bateria  | CAN            | CAN            | — (zero)       | — (zero)       |
| RPM               | CAN            | CAN            | — (zero)       | — (zero)       |
| Throttle          | ADS1115        | ADS1115        | ADC built-in   | ADC built-in   |
| Temperatura motor | ADS1115        | ADS1115        | ADC built-in   | ADS1115        |
| Temperatura ESC   | **CAN**        | **CAN**        | ADC built-in   | ADS1115        |

### 8.2 Classe responsável por cada dado

| Dado              | Hobbywing                              | T-Motor                             | XAG                                     |
|-------------------|----------------------------------------|-------------------------------------|-----------------------------------------|
| Tensão            | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan`    | `XagTelemetry` ← `BatteryVoltageSensor` |
| Corrente          | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan`    | `XagTelemetry` (zero)                   |
| RPM               | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan`    | `XagTelemetry` (zero)                   |
| Throttle          | `Throttle` ← lambda ADS1115            | `Throttle` ← lambda ADS1115         | `Throttle` ← lambda `analogRead`        |
| Temp motor        | `HobbywingTelemetry` ← `Temperature`  | `TmotorTelemetry` ← `Temperature`  | `XagTelemetry` ← `Temperature`          |
| **Temp ESC**      | **`HobbywingTelemetry` ← `HobbywingCan`** | **`TmotorTelemetry` ← `TmotorCan`** | **`XagTelemetry` ← `Temperature`**  |

---

## 9. Decisões de design e justificativas

### 9.1 Por que `#if` e não polimorfismo em runtime (vtable)?

O firmware compila uma implementação fixa por fabricante. Não há necessidade de selecionar implementações em runtime. Vtables têm custo de RAM e indireção em todo acesso. Para ESP32-C3, `#if` concentrado é a escolha correta — zero overhead, zero ambiguidade.

### 9.2 Por que `extern` global e não injeção de dependência?

Injeção de dependência via construtor (DI) é valiosa em sistemas com testes unitários e múltiplas configurações em runtime. Aqui, o contexto é firmware embarcado Arduino com um objeto por tipo. `extern` com escopo controlado (declarado em `.cpp`, não exposto em `.h`) é idiomático e adequado. Adiciona zero cerimônia sem perder clareza.

### 9.3 Por que `CanParser` e `CanSender` não são separados?

O sender precisa do ESC ID descoberto pelo parser. Eles compartilham estado: `transferId`, ESC Node ID, buffers de multi-frame. Separar em duas classes criaria acoplamento artificial. Uma única classe `HobbywingCan` com as duas responsabilidades é mais coesa.

### 9.4 Por que `HobbywingTelemetry` e `TmotorTelemetry` são separados se são parecidos?

São parecidos hoje. Se o T-Motor passar a usar sua leitura CAN de temperatura do motor (protocolo já suporta via Status 5, msg 1154) em vez do ADS1115, a separação garante que a mudança fica isolada em `TmotorTelemetry` sem afetar o Hobbywing.

### 9.5 Por que `BatteryVoltageSensor` é uma classe separada?

Centraliza a lógica de conversão ADC → millivolts (oversampling, divisor resistivo, calibração). Quando o XAG migrar para ADS1115, apenas a lambda injetada muda em `config.cpp` — sem tocar em `XagTelemetry`. Segue o mesmo padrão de `Temperature` e `Throttle`.

### 9.6 Por que pull e não push na atualização de telemetria?

Com pull, o fluxo de dados segue a chamada: `loop()` → `telemetry.update()` → `hobbywingTelemetry.update()` → getters do CAN driver. Em qualquer ponto do código é possível rastrear de onde vêm os dados. Com push, `HobbywingCan` escreveria diretamente em algum estado compartilhado a cada frame CAN recebido — o momento da escrita seria imprevisível e difícil de debugar.

### 9.7 Por que `TelemetryFormatter.h` e não funções em `TelemetryData.h`?

`TelemetryData.h` deve ser um struct passivo — incluído em todo lugar sem carregar dependências de `String` ou `Arduino.h`. `TelemetryFormatter.h` é incluído apenas por quem precisa formatar para exibição (`WebServer`, `Xctod`). A separação mantém `TelemetryData.h` leve e sem efeitos colaterais.

### 9.8 Por que macro `DEBUG_PRINTLN` e não `#if DEBUG` inline no código?

A macro isola a decisão de compilar ou não o log em um único ponto. O código dos drivers fica limpo e legível. Em produção, o compilador elimina completamente as chamadas sem overhead de branch ou string literals na flash.

---

## 10. Plano de migração incremental

A migração deve ser feita em etapas, cada uma compilando e funcionando antes de avançar. Nunca reescrever tudo de uma vez.

### Fase 1 — Sensores agnósticos: `Temperature`, `Throttle` e `TelemetryFormatter` (baixo risco)

**`Temperature`:** Remover o `#if IS_TMOTOR || IS_HOBBYWING` interno. Adicionar `ReadFn` no construtor. Atualizar instanciação em `config.cpp`.

**`Throttle`:** Mesma mudança — remover `#if` interno, adicionar `ReadFn` no construtor, atualizar `config.cpp`.

**`TelemetryFormatter.h`:** Criar o header com as funções `inline` de formatação. Remover as funções de `TelemetryData.h`. Atualizar os includes em `WebServer` e `Xctod`.

**Macro `DEBUG_PRINTLN`:** Adicionar a macro em `config.h`. Substituir `Serial.println()` de diagnóstico nos drivers CAN.

**Arquivos afetados:** `Temperature.h/.cpp`, `Throttle.h/.cpp`, `config.cpp`, novo `Telemetry/TelemetryFormatter.h`, `Telemetry/TelemetryData.h`, `WebServer/ControllerWebServer.cpp`, `Xctod/Xctod.cpp`, `config.h`  
**Risco:** Baixo — interfaces públicas não mudam, mudanças são localizadas

---

### Fase 2 — `BatteryVoltageSensor` (novo, baixo risco)

Criar a classe e migrar a lógica de conversão que hoje está inline no provider XAG.

**Arquivos afetados:** novo `Sensors/BatteryVoltageSensor.h/.cpp`, `config.cpp`  
**Risco:** Baixo — só XAG é afetado, Hobbywing e Tmotor não usam

---

### Fase 3 — `Canbus` (remover roteamento, médio risco)

Remover `isHobbywingEscMessage()`, `isTmotorEscMessage()`, as chamadas a `hobbywing.handle()` e `tmotor.handle()` de dentro de `Canbus`. Adicionar `receive()`. Atualizar `main.cpp` para fazer o roteamento e chamar `handle()` dos drivers via `#if`.

**Arquivos afetados:** `Canbus.h`, `Canbus.cpp`, `main.cpp`  
**Risco:** Médio — mudança no caminho crítico de recepção CAN. Validar que todos os frame types continuam sendo processados.

---

### Fase 4 — `HobbywingCan` e `TmotorCan` (renomear e reorganizar, médio risco)

Renomear `Hobbywing` → `HobbywingCan` e `Tmotor` → `TmotorCan`. Remover qualquer referência a `TelemetryData` ou `Canbus` de dentro dessas classes. Atualizar todos os `#include` e `extern`.

**Arquivos afetados:** `Hobbywing/` (renomear arquivos), `Tmotor/` (renomear arquivos), `config.h`, `config.cpp`, `main.cpp`  
**Risco:** Médio — rename em cascata, mas a lógica não muda

---

### Fase 5 — `HobbywingTelemetry`, `TmotorTelemetry`, `XagTelemetry` (novo, baixo risco)

Criar as três classes de telemetria de fabricante. Cada uma lê das fontes corretas e expõe getters. Verificar que `HobbywingTelemetry` e `TmotorTelemetry` não declaram `extern Temperature escTemp`.

**Arquivos afetados:** novos arquivos em `Hobbywing/`, `Tmotor/`, `Xag/`  
**Risco:** Baixo — são adições, não modificações

---

### Fase 6 — `Telemetry` (fachada, substituir Provider, alto risco)

Substituir o `TelemetryProvider` struct pela classe `Telemetry` com getters. `Telemetry.cpp` delega via `#if` para os `*Telemetry` de fabricante. Remover toda a pasta `Telemetry/` antiga (`*Provider.h/cpp`, `TelemetryProvider.h`).

**Arquivos afetados:** `Telemetry/` (refactor completo), `config.cpp`  
**Risco:** Alto — afeta todos os consumidores. Fazer junto com Fase 7.

---

### Fase 7 — Consumidores (médio risco)

Trocar acesso via `telemetry->getData()->campo` por `telemetry.getXxx()`. Remover `extern TelemetryProvider* telemetry` de cada consumidor.

**Arquivos afetados:** `Power.cpp`, `BatteryMonitor.cpp`, `Xctod.cpp`, `WebServer/ControllerWebServer.cpp`  
**Risco:** Médio — mudança em vários arquivos, mas mecânica (find & replace controlado)

---

### Fase 8 — Limpeza final

Verificar que não há referências a `TelemetryProvider`, `createHobbywingProvider`, `createTmotorProvider`, `createXagProvider`. Remover arquivos obsoletos. Verificar que as regras R1–R10 são respeitadas, em especial R6 (`#if IS_*` apenas nos lugares permitidos) e R9 (`escTemp` só no XAG).

---

## 11. Pontos em aberto

**XAG com ADS1115**  
Quando o ADS1115 chegar para o XAG, será necessário definir quais canais correspondem a motor temp, ESC temp e tensão da bateria. Adicionar defines em `config.h`:
```cpp
#if IS_XAG
#define ADS1115_MOTOR_TEMP_CHANNEL  0
#define ADS1115_ESC_TEMP_CHANNEL    1
#define ADS1115_BATTERY_CHANNEL     2
#endif
```
Com a arquitetura proposta, apenas as lambdas em `config.cpp` mudam — sem tocar nas classes `Temperature`, `BatteryVoltageSensor` ou `XagTelemetry`.

**T-Motor temperatura do motor via CAN**  
O protocolo T-Motor envia temperatura do motor via CAN (Status 5, msg 1154). Hoje o firmware usa ADS1115 como fonte primária. Quando houver confiança na leitura CAN, `TmotorTelemetry` pode ser ajustado para usar `tmotorCan.getMotorTemperature()` em vez de `motorTemp.getTemperature()`, sem nenhuma mudança em outras classes.

**WebServer e serialização JSON**  
O WebServer pode precisar de acesso a múltiplos campos de uma vez para serializar o JSON de status. Avaliar se `Telemetry::getData()` retornando `const TelemetryData&` faz sentido como método adicional à fachada, ou se os getters individuais são suficientes para todos os casos de uso.

**Testes**  
A arquitetura atual não prevê testes unitários. Se isso for desejado no futuro, o principal ponto de entrada seria mockar as lambdas `ReadFn` de `Temperature`, `Throttle` e `BatteryVoltageSensor`, e criar stubs simples para `HobbywingCan` e `TmotorCan`.

---

*Documento de planejamento — sujeito a revisão antes da implementação.*
