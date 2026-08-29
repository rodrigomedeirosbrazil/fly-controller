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
| **Telemetry**  | Telemetria    | Painel de voo ao vivo: bateria, tensão por célula, corrente, potência, motor, ESC e acelerador |
| **Firmware**   | Atualização   | Envio de arquivo `.bin` para atualizar o firmware    |
| **Logs**       | Logs          | Lista de arquivos de log, download e exclusão        |
| **Configuration** | Configuração | Todas as configurações persistentes do controlador   |

---

## 3. Dashboard (página inicial)

**URL:** `http://192.168.4.1/` ou `http://192.168.4.1`

Exibe:

- **Firmware Version** — Versão do firmware e data/hora da compilação.
- **Controller Type** — Tipo de controlador (Tmotor ou XAG).
- **Uptime** — Tempo de ligado do dispositivo em segundos.
- **Battery Voltage** — Tensão da bateria (em V), quando há telemetria.
- **Telemetry freshness** — Idade da última atualização de telemetria (em ms); indica se os dados estão atualizados.
- **System Status** — **ARMED** (armado) ou **DISARMED** (desarmado).
- **Telemetry state** — **LIVE** (dados recentes), **STALE** (dados antigos) ou **No telemetry data** (sem dados).

Os dados são atualizados automaticamente a cada 1 segundo.

---

## 4. Telemetria

**URL:** `http://192.168.4.1/telemetry`

Painel de voo do controlador, atualizado a cada 1 segundo. A tela é feita para
ser lida de relance: números grandes, dados prioritários primeiro, **sem
rolagem** e sem nada que mude de tamanho durante o voo — nenhum alerta empurra
ou redimensiona o restante da tela.

Funciona em pé ou deitado. Em paisagem a navegação vira um trilho lateral e os
quatro blocos entram numa fileira só; nada é escondido.

### Barra de status

| Elemento | O que mostra |
|---|---|
| **Ponto verde** | Telemetria ao vivo. Quando degrada, o rótulo aparece: **DESATUALIZADO** (última atualização há mais de 3 s) ou **SEM DADOS** (ESC não conectado / sem CAN). |
| **ARMADO / DESARMADO** | Estado do sistema. |
| **Chip vermelho com código** | Aparece só quando houve desarme por falha. Toque nele para abrir a explicação completa. Some ao rearmar. |
| **Tempo de vôo** | Conta enquanto armado e com o motor girando; sobrevive a desarmar e rearmar, zera ao reiniciar. |
| **Ícone de som** | Ativa ou silencia os beeps do buzzer no navegador. O primeiro toque desbloqueia o áudio (política de autoplay). Começa **silenciado**; o estado é lembrado entre visitas. |
| **Ícone de cadeado** | Mantém a tela do celular acesa enquanto a página está aberta (wake lock). O estado e a ajuda ficam na gaveta "Mais dados". |

### Mostrador da bateria

Ocupa a maior área da tela.

- **Ponteiro e número:** estado de carga por **coulomb counting** (%). O arco
  vermelho escuro no início do curso marca os últimos 20%.
- **Tensão:** tensão **por célula**. Vem da menor célula do BMS quando há BMS
  informando células; senão é a tensão do pack dividida pelo número de células
  configurado (§7.1). A gaveta diz qual das duas você está vendo.
- **Corrente:** corrente do pack (A). Quando não há sensor de corrente, esta
  célula desaparece e a tensão fica centralizada — nunca aparece "N/A".

### Instrumentos

| Bloco | Comportamento |
|---|---|
| **Potência** | Leitura numérica em kW, **sem ponteiro**: potência não tem fundo de escala, então um mostrador circular teria que inventar um. Some por completo quando o build não tem leitura de potência, e a fileira passa de três para duas colunas. |
| **Motor** | Mostrador de 0 até a temperatura máxima configurada, com a faixa de redução desenhada no próprio arco — dá para ver a margem, não só o valor. A terceira linha diz qual sensor produziu a leitura: **CAN** ou **NTC**. |
| **ESC** | Igual ao motor, com os limites do ESC. |

Os fundos de escala e as faixas de redução vêm das configurações de Proteção
Térmica (§7.2 e §7.3), lidos uma vez ao abrir a página. Um mostrador desenhado
contra o limite errado seria pior que nenhum mostrador.

### Acelerador

Barra grossa com a posição do acelerador em %. Quando há limitação de potência
ativa, um traço vermelho marca onde o teto entra — é o que explica por que
passar dali não acelera mais.

### Mais dados

O botão no rodapé abre uma gaveta por cima do painel, sem mexer no layout.
Ela reúne o que não é prioridade em voo:

- **Motor e ESC:** rotação (rpm), corrente do ESC, sensor da temperatura do motor.
- **Bateria e BMS:** SoC por tensão, tensão total do pack, origem da tensão por
  célula, estado do BMS, células mín–máx (mV), delta entre células, temperatura
  máxima do BMS.
- **Acelerador e sistema:** leitura bruta do acelerador, horímetro do motor,
  estado do "manter tela ativa".
- **Tempo de vôo** com o botão **Resetar** (pede o PIN de configuração).

### Estados de alerta

- **Potência reduzida:** o bloco que está causando a limitação fica vermelho, a
  potência ganha o selo **DISPONÍVEL xx %**, e a barra do acelerador ganha o
  traço do teto. Não há banner — a cor e o selo já dizem tudo, e um banner teria
  que aparecer e sumir, movendo os números justamente na hora de lê-los.
- **Sensor inválido:** o mostrador do sensor perdido não desenha arco nenhum e
  mostra um travessão com um selo dizendo o motivo (**DESATUALIZADO**,
  **INVÁLIDO**, **SEM DADO** ou **SENSOR MUDOU**). Nunca um número inventado
  para uma leitura em que não se pode confiar.
- **Desarme por falha:** chip vermelho na barra de status com o código; a
  explicação completa abre na gaveta.

### Dados ausentes

A página nunca mostra card vazio nem "N/A": o layout se fecha. Sem corrente, a
faixa da bateria fica com uma célula só; sem potência, a fileira de
instrumentos cai para duas colunas e motor e ESC crescem — que é a ênfase
certa, já que sem corrente a temperatura é o único indicador de esforço.

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

Os logs em CSV incluem, quando disponível, dados do BMS: **battery_temp_max** (temperatura máxima da bateria entre os NTCs), **cell_voltage_min_mv** e **cell_voltage_max_mv** (menor e maior tensão por célula em mV). Esses campos aparecem vazios se o BMS não estiver conectado.

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
| **Células em Série** | Número de células do pack. Faixa: **1 a 24**; padrão de fábrica **14**. Converte entre tensão por célula e tensão total nesta página, e alimenta a leitura de tensão por célula da Telemetria quando o BMS não informa as células individualmente. |
| **Minimum Voltage per Cell (V)** | Tensão **mínima** por célula (V). Faixa: **2,5 V a 4,5 V**. Abaixo dessa tensão (no total do pack), o controlador reduz a potência para proteger a bateria. O total é mostrado ao lado, já multiplicado pelo número de células (ex.: 3,15 V × 14 ≈ 44,1 V). |
| **Maximum Voltage per Cell (V)** | Tensão **máxima** por célula (V). Faixa: **2,5 V a 4,5 V**. Usado como referência para SoC por tensão. O total é mostrado da mesma forma (ex.: 4,15 V × 14 ≈ 58,1 V). |

**Dica:** Para LiPo, mínimo costuma ser 3,0 V a 3,15 V por célula; máximo 4,1 V a 4,2 V por célula. O pack de referência do Fly Controller é **14S**, mas qualquer contagem entre 1S e 24S pode ser configurada acima.

---

### 7.2 Temperatura do motor (Motor Temperature Settings)

| Configuração | Descrição e uso |
|--------------|------------------|
| **Maximum Motor Temperature (°C)** | Temperatura em que o motor é **totalmente desabilitado** (potência 0%). Faixa: **0 a 150 °C**. |
| **Motor Temperature Reduction Start (°C)** | Temperatura em que **começa** a redução linear de potência. Entre este valor e a temperatura máxima, a potência é reduzida gradualmente. Faixa: **0 a 150 °C**. Deve ser **menor** que a temperatura máxima. |

Exemplo: se “Reduction Start” = 80 °C e “Maximum” = 100 °C, entre 80 °C e 100 °C a potência cai linearmente de 100% a 0%.

Valores de fábrica: motor **80 °C → 100 °C**; ESC **80 °C → 110 °C** no Tmotor e **70 °C → 80 °C** no XAG. Os quatro valores são editáveis nesta página e ficam salvos na NVS.

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

### 7.5 Bluetooth BMS

| Configuração | Descrição e uso |
|--------------|------------------|
| **Status da conexão** | Mostra ao vivo o estado do BMS configurado (não configurado / conectando / conectado) e, quando recebendo dados, tensão, corrente, SoC, número de células, temperatura e delta entre células. |
| **BMS type** | Seleciona o backend Bluetooth do BMS. A interface suporta **JBD**, **Daly (D2 BLE)** e **JK BMS**. |
| **BMS Bluetooth address (MAC)** | Endereço MAC do BMS no formato **XX:XX:XX:XX:XX:XX** (6 bytes em hexadecimal separados por dois pontos). Pode ser digitado manualmente ou preenchido pelo scanner BLE da própria página. |
| **Scan for BMS** | Executa uma busca BLE manual por BMS compatíveis enquanto a página de configuração está aberta. O scanner lista apenas dispositivos que anunciam os serviços conhecidos do firmware: **0xFF00** para **JBD**, **0xFFF0** para **Daly (D2 BLE)** e **0xFFE0** para **JK BMS**. Ao selecionar um resultado, a interface preenche automaticamente o **tipo do BMS** e o **MAC**. |

**Nota:** Após alterar o MAC, salve a configuração e reinicie o controlador para que a nova conexão seja tentada.

---

### 7.6 Wi‑Fi (Wi-Fi Settings)

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
- A API de telemetria está em **GET /api/telemetry** (JSON). O objeto **availability** indica quais dados estão disponíveis (`current`, `rpm`, `powerKw`, `bms`, `bmsCells`). Campos numéricos como `rpm`, `escCurrentMa` e `powerKwX10` são omitidos quando indisponíveis (a página mostra N/A). O campo **disarmReason** indica o motivo do último desarme: vazio (nunca desarmou desde o boot), `MANUAL` (desarme normal pelo botão/interface), ou um código de falha: `THR ERR` (acelerador com fio inválido), `LINK ERR` (link do remote perdido), `MOT ERR` / `ESC ERR` / `BATT ERR` (sensor válido ao armar que se tornou inválido em voo) e `MOT SRC` (a fonte da temperatura do motor mudou entre CAN e NTC em voo). A página de Telemetria mostra um chip vermelho com o código na barra de status enquanto a falha estiver ativa e o sistema desarmado; tocar nele abre a explicação na gaveta. Quando o BMS está conectado, o objeto **bms** traz `tempMaxC`, `cellMinMv`, `cellMaxMv` e `cellDeltaMv`. O campo **buzzer** é um array com os últimos eventos de beep (até 8, do mais antigo ao mais recente): cada entrada tem `seq` (contador monotônico), `freq` (Hz), `onMs`, `offMs`, `reps` (255 = contínuo) e `active` (true = iniciado, false = parado). A página de Telemetria usa esses dados para reproduzir os beeps no navegador via Web Audio API. O objeto **signals** traz o estado de cada sensor que pode limitar a potência: `motorTemp`, `escTemp` e `battV`, cada um com um código de uma letra (`v` = válido, `s` = desatualizado, `i` = inválido, `a` = ausente). A página de Telemetria apaga o arco do mostrador e mostra um travessão com um selo colorido quando o código não é `v`. O objeto **signals** pode trazer ainda o campo opcional **motorTempSrc**, que indica qual sensor produziu a temperatura do motor: `can` (Status 5/PUSHCAN) ou `ntc` (termistor/ADS1115); ele é omitido em builds com uma única fonte, e a página de Telemetria o mostra como terceira linha dentro do mostrador do motor enquanto a leitura é válida. A página de Configuração usa **GET /config/values** (ler) e **POST /config/save** (gravar) com corpo JSON.
- Os limiares térmicos (**GET /api/config/thermal**) e o número de células (**GET /api/config/power**, campo `cellCount`) são lidos uma única vez ao abrir a Telemetria, não a cada segundo: eles só mudam quando o piloto edita as configurações, e o payload de 1 Hz fica menor por isso. São eles que definem o fundo de escala e a faixa vermelha dos mostradores de motor e ESC, e o divisor da tensão por célula.
- Os arquivos estáticos da interface (HTML, CSS e JS) são servidos **comprimidos em gzip** direto da flash, com o cabeçalho `Content-Encoding: gzip`. São gerados no momento da compilação por `scripts/gen_web_assets.py` a partir dos arquivos em `src/WebServer/Pages/`, que continuam sendo a fonte da verdade.

---

*Fly Controller — Manual da Interface Web. Documentação em português do Brasil.*
