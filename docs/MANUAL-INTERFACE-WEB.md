# Manual da Interface Web — Fly Controller

Este manual descreve a interface web do Fly Controller e todas as configurações disponíveis. Em português do Brasil.

---

## 1. Acesso à interface web

### Como conectar

1. **Ligue o controlador** (conecte a bateria).
2. O controlador cria um **ponto de acesso Wi‑Fi** com o nome: **`FlyController`**.
3. No celular, tablet ou computador, conecte-se à rede **FlyController** (sem senha).
4. Se o navegador não abrir sozinho, acesse no navegador: **http://192.168.4.1**

O endereço **192.168.4.1** é o IP do controlador na rede do ponto de acesso. Em alguns dispositivos, ao conectar na rede FlyController, um portal/página pode aparecer automaticamente.

### Desativação automática do Wi‑Fi

Se a opção **“Desativar Wi‑Fi após calibração do acelerador”** estiver ativada nas configurações (padrão), o ponto de acesso e o servidor web **são desligados automaticamente** depois que a calibração do acelerador é concluída. Para usar a interface de novo, é preciso **reiniciar o controlador** (desligar e ligar a bateria).

---

## 2. Navegação e páginas

No topo da tela há uma barra de navegação com os links:

| Link           | Página        | Descrição breve                                      |
|----------------|---------------|------------------------------------------------------|
| **Dashboard**  | Página inicial| Visão geral: versão, tipo de controlador, tensão, estado |
| **Telemetry**  | Telemetria    | Dados ao vivo: bateria, potência, throttle, motor, ESC |
| **Firmware**   | Atualização   | Envio de arquivo `.bin` para atualizar o firmware    |
| **Logs**       | Logs          | Lista de arquivos de log, download e exclusão        |
| **Configuration** | Configuração | Todas as configurações persistentes do controlador   |

---

## 3. Dashboard (página inicial)

**URL:** `http://192.168.4.1/` ou `http://192.168.4.1`

Exibe:

- **Firmware Version** — Versão do firmware e data/hora da compilação.
- **Controller Type** — Tipo de controlador (Hobbywing, Tmotor ou XAG).
- **Uptime** — Tempo de ligado do dispositivo em segundos.
- **Battery Voltage** — Tensão da bateria (em V), quando há telemetria.
- **Telemetry freshness** — Idade da última atualização de telemetria (em ms); indica se os dados estão atualizados.
- **System Status** — **ARMED** (armado) ou **DISARMED** (desarmado).
- **Telemetry state** — **LIVE** (dados recentes), **STALE** (dados antigos) ou **No telemetry data** (sem dados).

Os dados são atualizados automaticamente a cada 1 segundo.

---

## 4. Telemetria (Live Telemetry)

**URL:** `http://192.168.4.1/telemetry`

Mostra os dados ao vivo do sistema, atualizados a cada 1 segundo.

### Badge de status

- **NO DATA** — Ainda não há dados de telemetria (ex.: ESC não conectado ou sem CAN).
- **STALE** — Última atualização há mais de 3 segundos.
- **LIVE** — Dados sendo atualizados normalmente.

### Campos exibidos

| Campo                | Descrição |
|----------------------|-----------|
| **Battery Voltage**  | Tensão da bateria (V). |
| **Battery SoC (CC)** | Estado de carga por coulomb counting (%). |
| **Battery SoC (Voltage)** | Estado de carga estimado pela tensão (%). |
| **Power**            | Potência instantânea (kW), quando há sensor de corrente. |
| **Limit**            | Limite de potência aplicado pelos sensores (%). |
| **Throttle**         | Posição do acelerador (%). |
| **Raw**              | Valor bruto do acelerador (para diagnóstico). |
| **Motor**            | Temperatura do motor (°C). |
| **RPM**              | Rotação do motor (rpm), quando disponível. |
| **ESC**              | Temperatura do ESC (°C). |
| **ESC Current**      | Corrente do ESC (A), quando há sensor. |
| **System**           | ARMED ou DISARMED. |
| **Last update**      | Há quantos milissegundos os dados foram atualizados. |

Em builds sem sensor de corrente (ou sem telemetria de corrente), campos como **Power**, **RPM** e **ESC Current** podem aparecer como **N/A** ou **0**.

---

## 5. Atualização de firmware (Firmware Update)

**URL:** `http://192.168.4.1/firmware`

Permite atualizar o firmware do controlador via navegador.

### Passo a passo

1. Acesse a página **Firmware**.
2. Clique em **Escolher arquivo** e selecione o arquivo **`.bin`** do novo firmware.
3. Clique em **Update Firmware**.
4. Aguarde a mensagem de sucesso. O dispositivo **reinicia sozinho** após uma atualização bem-sucedida.
5. Reconecte-se à rede **FlyController** e acesse de novo **http://192.168.4.1** para confirmar a nova versão no Dashboard.

**Atenção:** Não desconecte a alimentação nem feche a página durante o envio do arquivo. Uma falha no meio do processo pode exigir atualização via serial/OTA de recuperação.

---

## 6. Logs (Data Logs)

**URL:** `http://192.168.4.1/logs-page`

Lista os arquivos de log armazenados na memória interna (LittleFS). Apenas arquivos **`.txt`** são listados.

### Ações

- **Download** — Baixa o arquivo de log para o seu dispositivo.
- **Delete** — Remove o arquivo do controlador (confirmação pedida antes).

A tabela mostra o **nome do arquivo** e o **tamanho**. A lista é recarregada ao abrir a página; após excluir um arquivo, a tabela é atualizada.

---

## 7. Configuração (Configuration)

**URL:** `http://192.168.4.1/config`

Nesta página você altera as configurações persistentes do controlador. As alterações são salvas na memória do ESP32 (Preferences) e permanecem após desligar e ligar.

Ao abrir a página, os **valores atuais** são carregados automaticamente. Depois de alterar os campos, clique em **Save Configuration** para salvar. Uma mensagem de sucesso ou erro aparece abaixo do botão.

---

### 7.1 Bateria (Battery Settings)

| Configuração | Descrição e uso |
|--------------|------------------|
| **Battery Capacity (Ah)** | Capacidade nominal da bateria em Ah. Opções pré-definidas: **18 Ah**, **34 Ah**, **65 Ah**. Se escolher **Custom**, aparece um campo para informar de **1 a 200 Ah**. Usado para coulomb counting e estimativa de SoC. |
| **Minimum Voltage per Cell (V)** | Tensão **mínima** por célula (V). Faixa: **2,5 V a 4,5 V**. Abaixo dessa tensão (no total do pack), o controlador reduz a potência para proteger a bateria. O total para 14 células é mostrado ao lado (ex.: 3,15 V × 14 ≈ 44,1 V). |
| **Maximum Voltage per Cell (V)** | Tensão **máxima** por célula (V). Faixa: **2,5 V a 4,5 V**. Usado como referência para SoC por tensão. O total para 14 células é mostrado (ex.: 4,15 V × 14 ≈ 58,1 V). |

**Dica:** Para LiPo, mínimo costuma ser 3,0 V a 3,15 V por célula; máximo 4,1 V a 4,2 V por célula. O pack do Fly Controller é **14S** (14 células em série).

---

### 7.2 Temperatura do motor (Motor Temperature Settings)

| Configuração | Descrição e uso |
|--------------|------------------|
| **Maximum Motor Temperature (°C)** | Temperatura em que o motor é **totalmente desabilitado** (potência 0%). Faixa: **0 a 150 °C**. |
| **Motor Temperature Reduction Start (°C)** | Temperatura em que **começa** a redução linear de potência. Entre este valor e a temperatura máxima, a potência é reduzida gradualmente. Faixa: **0 a 150 °C**. Deve ser **menor** que a temperatura máxima. |

Exemplo: se “Reduction Start” = 50 °C e “Maximum” = 60 °C, entre 50 °C e 60 °C a potência cai linearmente de 100% a 0%.

---

### 7.3 Temperatura do ESC (ESC Temperature Settings)

| Configuração | Descrição e uso |
|--------------|------------------|
| **Maximum ESC Temperature (°C)** | Temperatura em que o ESC é **totalmente desabilitado** (potência 0%). Faixa: **0 a 150 °C**. |
| **ESC Temperature Reduction Start (°C)** | Temperatura em que **começa** a redução linear de potência do ESC. Faixa: **0 a 150 °C**. Deve ser **menor** que a temperatura máxima. |

O comportamento é análogo ao do motor: entre “Reduction Start” e “Maximum” a potência é reduzida linearmente.

---

### 7.4 Controle de potência (Power Control Settings)

| Configuração | Descrição e uso |
|--------------|------------------|
| **Enable Power Control** | **Marcado:** o controlador limita a potência com base na tensão da bateria e nas temperaturas do motor e do ESC (o limite mais restritivo vale). **Desmarcado:** não há limite por esses sensores (potência total disponível, respeitando apenas o acelerador). **Padrão:** ativado. |

Quando ativado, o “Limit” exibido na página de Telemetria reflete esse limite (%). Quando desativado, o limite mostrado pode ser 100% independente dos sensores.

---

### 7.5 Wi‑Fi (Wi-Fi Settings)

| Configuração | Descrição e uso |
|--------------|------------------|
| **Disable Wi-Fi after throttle calibration** | **Marcado:** após a calibração do acelerador ser concluída, o ponto de acesso e o servidor web são **desligados** automaticamente (economia de energia e menos interferência). Para usar a interface de novo é preciso **reiniciar** o controlador. **Desmarcado:** o Wi‑Fi permanece ligado até desligar a alimentação. **Padrão:** ativado. |

---

## 8. Resumo das URLs

| Página        | URL                    |
|---------------|------------------------|
| Dashboard     | http://192.168.4.1/    |
| Telemetria    | http://192.168.4.1/telemetry |
| Firmware     | http://192.168.4.1/firmware |
| Logs          | http://192.168.4.1/logs-page |
| Configuração  | http://192.168.4.1/config   |

---

## 9. Observações técnicas

- A interface é servida pelo próprio controlador (ESP32); não depende de internet.
- O ponto de acesso **FlyController** não usa senha; qualquer dispositivo próximo pode conectar. Use em ambiente controlado.
- As configurações são validadas no servidor (por exemplo, capacidade 1000–200000 mAh, tensões e temperaturas dentro das faixas). Valores fora do permitido são rejeitados com mensagem de erro.
- A API de telemetria está em **GET /api/telemetry** (JSON). A página de Configuração usa **GET /config/values** (ler) e **POST /config/save** (gravar) com corpo JSON.

---

*Fly Controller — Manual da Interface Web. Documentação em português do Brasil.*
