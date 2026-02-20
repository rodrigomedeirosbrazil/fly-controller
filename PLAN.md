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

| Fabricante | Motor Temp | ESC Temp | Tensão Bateria | Corrente | RPM | CAN |
|------------|------------|----------|----------------|----------|-----|-----|
| Hobbywing  | ADS1115    | CAN      | CAN            | CAN      | CAN | Sim |
| T-Motor    | ADS1115    | CAN      | CAN            | CAN      | CAN | Sim |
| XAG        | ADC / ADS1115 (em breve) | ADC / ADS1115 (em breve) | ADC / ADS1115 (em breve) | — | — | Não |

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
A classe `Telemetry` atual acumula: leitura de hardware, parse de CAN, regra de negócio, armazenamento de estado e envio de NodeStatus. Isso viola o princípio de responsabilidade única.

**`#if` espalhado por múltiplos arquivos**  
Diretivas de compilação condicional aparecem dentro de `Temperature.cpp`, `Canbus.cpp`, `Power.cpp`, `BatteryMonitor.cpp`, `Xctod.cpp` e `main.cpp`. Cada novo fabricante exige modificações em vários arquivos.

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
Classes como `Temperature` e `BatteryVoltageSensor` não sabem se estão num Hobbywing ou XAG. Recebem uma função de leitura ADC pelo construtor.

**Pull, não push**  
`Telemetry::update()` chama ativamente `*Telemetry::update()` e lê os dados. O fluxo de dados segue a chamada, tornando o comportamento previsível e fácil de debugar.

---

## 4. Estrutura de pastas

```
src/
│
├── Hobbywing/
│   ├── HobbywingCan.h          ← interface do protocolo DroneCAN Hobbywing
│   ├── HobbywingCan.cpp        ← parse de frames + envio de comandos
│   ├── HobbywingTelemetry.h    ← interface de agregação de telemetria
│   └── HobbywingTelemetry.cpp  ← lê HobbywingCan + ADS1115
│
├── Tmotor/
│   ├── TmotorCan.h             ← interface do protocolo TM-UAVCAN v2.3
│   ├── TmotorCan.cpp           ← parse de frames + envio de comandos
│   ├── TmotorTelemetry.h       ← interface de agregação de telemetria
│   └── TmotorTelemetry.cpp     ← lê TmotorCan + ADS1115
│
├── Xag/
│   ├── XagTelemetry.h          ← interface de agregação de telemetria
│   └── XagTelemetry.cpp        ← lê ADC (preparado para ADS1115)
│
├── Telemetry/
│   ├── Telemetry.h             ← fachada pública (getters)
│   ├── Telemetry.cpp           ← #if seleciona implementação de fabricante
│   └── TelemetryData.h         ← struct de dados puro, sem lógica
│
├── Canbus/
│   ├── Canbus.h                ← protocolo DroneCAN genérico
│   ├── Canbus.cpp              ← receive(), sendNodeStatus(), GetNodeInfo
│   └── CanUtils.h              ← utilitários de frame CAN (sem mudança)
│
├── Sensors/
│   ├── BatteryVoltageSensor.h  ← conversão ADC → millivolts (ReadFn)
│   └── BatteryVoltageSensor.cpp
│
├── Temperature/
│   ├── Temperature.h           ← NTC via Steinhart-Hart (ReadFn)
│   └── Temperature.cpp         ← sem #if interno
│
├── ADS1115/                    ← sem mudança
├── Power/                      ← consome Telemetry (fachada)
├── BatteryMonitor/             ← consome Telemetry (fachada)
├── Xctod/                      ← consome Telemetry (fachada)
├── WebServer/                  ← consome Telemetry (fachada)
├── Buzzer/                     ← sem mudança
├── Button/                     ← sem mudança
├── Throttle/                   ← sem mudança
├── Settings/                   ← sem mudança
├── Logger/                     ← sem mudança
│
├── config.h                    ← externs globais, defines de pinos
├── config.cpp                  ← instanciação de todos os objetos globais
├── config_controller.h         ← IS_XAG / IS_HOBBYWING / IS_TMOTOR
└── main.cpp                    ← setup() e loop() limpos
```

---

## 5. Descrição de cada componente

### 5.1 `HobbywingCan` / `TmotorCan`

**O que é:** A classe de comunicação CAN do fabricante. É a única que toca em `twai_message_t` dentro do contexto do fabricante.

**Responsabilidades:**
- Fazer parse de todos os frames CAN recebidos do ESC
- Armazenar o estado interno extraído (RPM, tensão, corrente, ESC temp, ESC ID)
- Enviar comandos: throttle, configuração de LED, direção, request de ESC ID
- Gerenciar o `transferId` e a cadência de 400 Hz de throttle via `handle()`

**Não é responsabilidade:**
- Montar `TelemetryData`
- Saber o que é temperatura do motor (isso vem do ADS1115, não do CAN)
- Chamar `Canbus` diretamente — recebe o frame já extraído

**Interface pública esperada (Hobbywing como exemplo):**
```cpp
class HobbywingCan {
public:
    // Chamado pelo main.cpp quando chega um frame CAN
    void parse(twai_message_t* msg);

    // Cadência periódica: envia throttle a 400 Hz
    void handle();

    // Comandos ao ESC
    void requestEscId();
    void setThrottleSource(uint8_t source);
    void setLedColor(uint8_t color, uint8_t blink = 0);
    void setRawThrottle(int16_t throttle);

    // Estado interno — lido por HobbywingTelemetry
    uint16_t getRpm()                    const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const;
    uint8_t  getEscTemperature()          const;
    bool     hasTelemetry()               const;
    bool     isReady()                    const;
};
```

**Diferença para o código atual:**  
A classe `Hobbywing` atual já faz parse e send — está correto. A mudança é principalmente de nome (`Hobbywing` → `HobbywingCan`) e remoção de qualquer referência a `TelemetryData` ou `Canbus` de dentro dela.

---

### 5.2 `HobbywingTelemetry` / `TmotorTelemetry`

**O que é:** O agregador de telemetria do fabricante. Sabe de onde vem cada dado e monta um estado coerente.

**Responsabilidades:**
- Ler dados CAN via getters do `HobbywingCan` / `TmotorCan`
- Ler temperatura do motor via objeto `Temperature` (ADS1115)
- Montar e manter o estado interno atualizado
- Responder a `update()` e expor os dados via getters

**Não é responsabilidade:**
- Fazer parse de CAN
- Saber detalhes do protocolo de comunicação
- Conhecer `TelemetryData` diretamente — expõe getters, a fachada `Telemetry` monta o struct

**Acesso a dependências:**  
Via `extern` global (opção B — idiomático para Arduino):
```cpp
// HobbywingTelemetry.cpp
extern HobbywingCan hobbywingCan;
extern Temperature  motorTemp;
```

**Interface pública esperada:**
```cpp
class HobbywingTelemetry {
public:
    void update();

    bool     hasData()                        const;
    uint16_t getBatteryVoltageMilliVolts()    const;
    uint32_t getBatteryCurrentMilliAmps()     const;
    uint16_t getRpm()                         const;
    int32_t  getMotorTempMilliCelsius()       const;
    int32_t  getEscTempMilliCelsius()         const;
    unsigned long getLastUpdate()             const;
};
```

---

### 5.3 `XagTelemetry`

**O que é:** O agregador de telemetria do XAG. Sem CAN, todas as fontes são locais.

**Responsabilidades:**
- Ler temperatura do motor via `Temperature` (ADC ou ADS1115)
- Ler temperatura do ESC via `Temperature` (ADC ou ADS1115)
- Ler tensão da bateria via `BatteryVoltageSensor` (ADC ou ADS1115)
- Expor os dados via getters

**Nota sobre migração para ADS1115:**  
O XAG atualmente usa ADC built-in. Em breve receberá ADS1115. A classe `XagTelemetry` não precisa mudar — apenas as lambdas injetadas em `Temperature` e `BatteryVoltageSensor` mudam em `config.cpp`:

```cpp
// Hoje (ADC built-in)
Temperature motorTemp([]() { return analogRead(MOTOR_TEMPERATURE_PIN); });

// Depois (ADS1115)
Temperature motorTemp([]() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); });
```

**Acesso a dependências:**
```cpp
// XagTelemetry.cpp
extern Temperature         motorTemp;
extern Temperature         escTemp;
extern BatteryVoltageSensor batterySensor;
```

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
// Telemetry.cpp
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

void Telemetry::update()                        { IMPL.update(); }
bool Telemetry::hasData()                 const { return IMPL.hasData(); }
uint16_t Telemetry::getBatteryVoltageMilliVolts() const { return IMPL.getBatteryVoltageMilliVolts(); }
// ... demais getters
```

**Interface pública:**
```cpp
class Telemetry {
public:
    void init();
    void update();

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
- Receber frames brutos (`receive()`) e devolver ao chamador
- Enviar `NodeStatus` periódico (protocolo DroneCAN genérico)
- Responder a `GetNodeInfo` (protocolo DroneCAN genérico)
- Detectar Node ID do ESC via `NodeStatus` recebido

**Não é responsabilidade:**
- Conhecer `Hobbywing`, `Tmotor` ou qualquer protocolo de fabricante
- Rotear mensagens para drivers específicos
- Chamar `handle()` de nenhum fabricante

**`receive()` — comportamento:**  
Trata internamente `NodeStatus` e `GetNodeInfo` (protocolo genérico). Devolve `true` e preenche `outMsg` apenas para mensagens de dados de fabricante que o chamador deve processar.

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

    // Cadência de throttle: cada driver gerencia o próprio timing
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

**O que é:** Classe de leitura de sensor NTC via equação de Steinhart-Hart.

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
// Hobbywing e Tmotor: ADS1115
Temperature motorTemp([]() {
    return ads1115.readChannel(ADS1115_TEMP_CHANNEL);
});

// XAG atual: ADC built-in
Temperature motorTemp([]() { return analogRead(MOTOR_TEMPERATURE_PIN); });
Temperature escTemp  ([]() { return analogRead(ESC_TEMPERATURE_PIN); });

// XAG futuro: ADS1115
Temperature motorTemp([]() { return ads1115.readChannel(ADS1115_MOTOR_CHANNEL); });
Temperature escTemp  ([]() { return ads1115.readChannel(ADS1115_ESC_CHANNEL); });
```

A classe `Temperature` não precisa mudar para a migração do XAG para ADS1115.

---

### 5.7 `BatteryVoltageSensor` (novo)

**O que é:** Classe dedicada à leitura de tensão da bateria via divisor resistivo. Análoga à `Temperature` — recebe `ReadFn`.

**Motivação:**  
Hoje a lógica de conversão ADC → millivolts (divisor resistivo, calibração, oversampling) está inline em `XagTelemetry`. Quando o XAG migrar para ADS1115, essa lógica precisaria ser alterada dentro da classe de telemetria. Extraindo para `BatteryVoltageSensor`, a mudança fica isolada na lambda injetada.

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

### 5.8 `TelemetryData`

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

**Nota:** As funções `formatVoltage()`, `formatTemperature()` que estavam inline neste header devem ser movidas para onde são usadas (WebServer ou um utilitário local), mantendo o struct completamente passivo.

---

### 5.9 `config.cpp`

**O que é:** O único lugar onde objetos globais são instanciados e onde o `#if` de fabricante aparece para definir quais objetos existem.

**Responsabilidade:**  
- Instanciar todos os objetos globais
- Injetar as lambdas corretas em `Temperature` e `BatteryVoltageSensor` conforme o firmware
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
                        │ frame de dados
                        ▼
                   main.cpp
                  checkCanbus()
                        │
                        ▼
             HobbywingCan::parse(msg)
             (atualiza estado interno:
              RPM, tensão, corrente,
              ESC temp, ESC ID)
                        
              [a cada loop]
                        
             Telemetry::update()
                  │ pull
                  ▼
       HobbywingTelemetry::update()
          ├── hobbywingCan.getRpm()
          ├── hobbywingCan.getBatteryVoltageMilliVolts()
          ├── hobbywingCan.getBatteryCurrentMilliAmps()
          ├── hobbywingCan.getEscTemperature()
          └── motorTemp.getTemperature()  (ADS1115)
                  │
                  ▼
            estado interno atualizado
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
                        
         Temperature::handle()     ← motorTemp (ADC/ADS1115)
         Temperature::handle()     ← escTemp   (ADC/ADS1115)
         BatteryVoltageSensor::handle() ← (ADC/ADS1115)

             Telemetry::update()
                  │ pull
                  ▼
          XagTelemetry::update()
          ├── motorTemp.getTemperature()
          ├── escTemp.getTemperature()
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
             Throttle::getThrottleRaw()
                        │
                        ▼
               Power::getPwm()
                        │
               esc.writeMicroseconds(pwm)  ← sinal PWM físico

               HobbywingCan::handle()  ← também envia via CAN a 400 Hz
               (chamado em checkCanbus() no main.cpp)
```

---

## 7. Regras arquiteturais

Estas regras devem ser respeitadas durante a implementação. Qualquer violação indica que algo foi modelado no lugar errado.

| Regra | Descrição |
|-------|-----------|
| **R1** | `HobbywingCan` e `TmotorCan` são os únicos que constroem ou interpretam `twai_message_t` dentro do contexto do fabricante |
| **R2** | `Canbus` não conhece `HobbywingCan`, `TmotorCan`, `Hobbywing` ou `Tmotor` |
| **R3** | `*Telemetry` não faz parse de CAN — só lê getters |
| **R4** | `Telemetry` (fachada) é o único ponto de acesso a dados para `Power`, `BatteryMonitor`, `Xctod`, `WebServer` |
| **R5** | `Temperature` e `BatteryVoltageSensor` não contêm `#if IS_*` |
| **R6** | `#if IS_HOBBYWING / IS_TMOTOR / IS_XAG` aparece apenas em `config.cpp`, `Telemetry.cpp` e `main.cpp` |
| **R7** | `Canbus::handle()` não chama `handle()` de nenhum driver de fabricante |
| **R8** | Nenhuma classe de fabricante escreve diretamente em `TelemetryData` — apenas expõe getters |

---

## 8. Fontes de dados por fabricante

Tabela detalhada de onde cada dado é obtido em cada firmware:

| Dado | Hobbywing | T-Motor | XAG (atual) | XAG (futuro) |
|------|-----------|---------|-------------|--------------|
| Tensão bateria | `HobbywingCan` (CAN) | `TmotorCan` (CAN) | `analogRead` | ADS1115 |
| Corrente bateria | `HobbywingCan` (CAN) | `TmotorCan` (CAN) | — (zero) | — (zero) |
| RPM | `HobbywingCan` (CAN) | `TmotorCan` (CAN) | — (zero) | — (zero) |
| Temperatura motor | `Temperature` (ADS1115) | `Temperature` (ADS1115) | `Temperature` (ADC) | `Temperature` (ADS1115) |
| Temperatura ESC | `HobbywingCan` (CAN) | `TmotorCan` (CAN) | `Temperature` (ADC) | `Temperature` (ADS1115) |

**Classe responsável por cada dado:**

| Dado | Hobbywing | T-Motor | XAG |
|------|-----------|---------|-----|
| Tensão | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan` | `XagTelemetry` ← `BatteryVoltageSensor` |
| Corrente | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan` | `XagTelemetry` (zero) |
| RPM | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan` | `XagTelemetry` (zero) |
| Temp motor | `HobbywingTelemetry` ← `Temperature` | `TmotorTelemetry` ← `Temperature` | `XagTelemetry` ← `Temperature` |
| Temp ESC | `HobbywingTelemetry` ← `HobbywingCan` | `TmotorTelemetry` ← `TmotorCan` | `XagTelemetry` ← `Temperature` |

---

## 9. Decisões de design e justificativas

### 9.1 Por que `#if` e não polimorfismo em runtime (vtable)?

O firmware compila uma implementação fixa por fabricante. Não há necessidade de selecionar implementações em runtime. Vtables têm custo de RAM e indireção em todo acesso. Para ESP32-C3, `#if` concentrado é a escolha correta — zero overhead, zero ambiguidade.

### 9.2 Por que `extern` global e não injeção de dependência?

Injeção de dependência via construtor (DI) é valiosa em sistemas com testes unitários e múltiplas configurações em runtime. Aqui, o contexto é firmware embarcado Arduino com um objeto por tipo. `extern` com escopo controlado (declarado em `.cpp`, não exposto em `.h`) é idiomático e adequado. Adiciona zero cerimônia sem perder clareza.

### 9.3 Por que `CanParser` e `CanSender` não são separados?

O sender precisa do ESC ID descoberto pelo parser. Eles compartilham estado — `transferId`, ESC Node ID, buffers de multi-frame. Separar em duas classes criaria acoplamento artificial entre elas. Uma única classe `HobbywingCan` com as duas responsabilidades (parse + send) é mais coesa.

### 9.4 Por que `HobbywingTelemetry` e `TmotorTelemetry` são separados se são parecidos?

São parecidos hoje. Se o T-Motor receber um novo sensor ou mudar a fonte de temperatura do motor (já tem `getMotorTemperature()` via CAN no protocolo), a separação garante que a mudança fica isolada. O custo de ter dois arquivos é menor do que o risco de uma mudança em um fabricante afetar o outro.

### 9.5 Por que `BatteryVoltageSensor` é uma classe separada?

Centraliza a lógica de conversão ADC → millivolts (oversampling, divisor resistivo, calibração). Quando o XAG migrar para ADS1115, apenas a lambda injetada muda — sem tocar em `XagTelemetry`. Segue o mesmo padrão de `Temperature`.

### 9.6 Por que pull e não push na atualização de telemetria?

Com pull, o fluxo de dados segue a chamada: `loop()` → `telemetry.update()` → `hobbywingTelemetry.update()` → getters do CAN driver. Em qualquer ponto do código é possível saber de onde vêm os dados sem rastrear callbacks. Com push, `HobbywingCan` escreveria diretamente em algum estado compartilhado a cada frame CAN recebido — o momento da escrita seria imprevisível e difícil de debugar.

---

## 10. Plano de migração incremental

A migração deve ser feita em etapas, cada uma compilando e funcionando antes de avançar. Nunca reescrever tudo de uma vez.

### Fase 1 — `Temperature` (baixo risco, alto impacto)

Remover o `#if IS_TMOTOR || IS_HOBBYWING` interno. Adicionar `ReadFn` no construtor. Atualizar instanciação em `config.cpp`.

**Arquivos afetados:** `Temperature.h`, `Temperature.cpp`, `config.cpp`  
**Risco:** Baixo — mudança localizada, interface pública não muda (`getTemperature()` continua igual)

---

### Fase 2 — `BatteryVoltageSensor` (novo)

Criar a classe e migrar a lógica de conversão que hoje está inline em `XagProvider.cpp`.

**Arquivos afetados:** novo `Sensors/BatteryVoltageSensor.h/.cpp`, `config.cpp`  
**Risco:** Baixo — só XAG é afetado, Hobbywing e Tmotor não usam

---

### Fase 3 — `Canbus` (remover roteamento)

Remover `isHobbywingEscMessage()`, `isTmotorEscMessage()`, as chamadas a `hobbywing.handle()` e `tmotor.handle()`. Adicionar `receive()`. Atualizar `main.cpp` para fazer o roteamento via `#if`.

**Arquivos afetados:** `Canbus.h`, `Canbus.cpp`, `main.cpp`  
**Risco:** Médio — mudança no caminho crítico de recepção CAN. Validar que todos os frame types continuam sendo processados.

---

### Fase 4 — `HobbywingCan` e `TmotorCan` (renomear e reorganizar)

Renomear `Hobbywing` → `HobbywingCan` e `Tmotor` → `TmotorCan`. Remover qualquer referência a `TelemetryData` ou `Canbus` de dentro dessas classes. Atualizar todos os `#include` e `extern`.

**Arquivos afetados:** `Hobbywing/` (renomear arquivos), `Tmotor/` (renomear arquivos), `config.h`, `config.cpp`, `main.cpp`  
**Risco:** Médio — rename em cascata, mas a lógica não muda

---

### Fase 5 — `HobbywingTelemetry`, `TmotorTelemetry`, `XagTelemetry` (novo)

Criar as três classes de telemetria de fabricante. Cada uma lê das fontes corretas e expõe getters.

**Arquivos afetados:** novos arquivos em `Hobbywing/`, `Tmotor/`, `Xag/`  
**Risco:** Baixo — são adições, não modificações

---

### Fase 6 — `Telemetry` (fachada, substituir Provider)

Substituir `TelemetryProvider` struct pela classe `Telemetry` com getters. `Telemetry.cpp` delega via `#if` para os `*Telemetry` de fabricante. Remover toda a pasta `Telemetry/` antiga (`*Provider.h/cpp`, `TelemetryProvider.h`).

**Arquivos afetados:** `Telemetry/` (refactor completo), `config.cpp`  
**Risco:** Alto — afeta todos os consumidores. Fazer junto com Fase 7.

---

### Fase 7 — Consumidores (Power, BatteryMonitor, Xctod, WebServer)

Trocar acesso via `telemetry->getData()->campo` por `telemetry.getXxx()`. Remover `extern TelemetryProvider* telemetry` de cada consumidor.

**Arquivos afetados:** `Power.cpp`, `BatteryMonitor.cpp`, `Xctod.cpp`, `WebServer/ControllerWebServer.cpp`  
**Risco:** Médio — mudança em vários arquivos, mas mecânica (find & replace controlado)

---

### Fase 8 — Limpeza final

Verificar que não há referências a `TelemetryProvider`, `createHobbywingProvider`, `createTmotorProvider`, `createXagProvider`. Remover arquivos obsoletos. Verificar que `#if IS_*` aparece apenas nos lugares permitidos (R6).

---

## 11. Pontos em aberto

Questões que podem surgir durante a implementação e que vale registrar:

**XAG com ADS1115**  
Quando o ADS1115 chegar para o XAG, será necessário definir quais canais do chip correspondem a motor temp, ESC temp e tensão da bateria. Esses canais devem ser adicionados como defines em `config.h` análogos aos existentes para Hobbywing/Tmotor:
```cpp
#if IS_XAG
#define ADS1115_MOTOR_TEMP_CHANNEL  0
#define ADS1115_ESC_TEMP_CHANNEL    1
#define ADS1115_BATTERY_CHANNEL     2
#endif
```

**T-Motor temperatura do motor via CAN**  
O protocolo T-Motor envia temperatura do motor via CAN (Status 5, msg 1154). Hoje o firmware usa ADS1115 como fonte primária. Quando houver confiança na leitura CAN, `TmotorTelemetry` pode ser ajustado para usar `tmotorCan.getMotorTemperature()` em vez de `motorTemp.getTemperature()`, sem nenhuma mudança em outras classes.

**WebServer e `TelemetryData`**  
O WebServer pode ainda precisar de acesso ao struct completo para serializar o JSON de uma vez. Avaliar se `Telemetry::getData()` retornando `const TelemetryData&` faz sentido como método adicional, ou se getters individuais são suficientes.

**Testes**  
A arquitetura atual não prevê testes unitários. Se isso for desejado no futuro, o principal ponto de entrada seria mockar as lambdas `ReadFn` de `Temperature` e `BatteryVoltageSensor`, e criar stubs para `HobbywingCan` e `TmotorCan`.

---

*Documento gerado como referência para implementação futura. Versão inicial — sujeito a revisão.*
