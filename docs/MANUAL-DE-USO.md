# Manual de Uso — Fly Controller

Manual do usuário do controlador de voo para paramotores elétricos (Fly Controller). Em português do Brasil.

---

## 1. Início do sistema e ligação da bateria

1. **Conecte a bateria** ao controlador conforme a polaridade indicada no equipamento.
2. Ao alimentar o sistema, o controlador **inicia automaticamente**.
3. Você ouvirá um **beep de partida** no buzzer, indicando que o sistema ligou.
4. O **acelerador (throttle) ainda não está calibrado**. É necessário calibrar antes de armar (ver seção 3).

**Importante:** Não tente armar o sistema antes de concluir a calibração do acelerador.

---

## 2. Calibração do acelerador

O acelerador usa um sensor (ex.: Hall) que precisa ser calibrado toda vez que sistema é ligado. A calibração define o ponto mínimo e máximo do curso do acelerador.

### Passo a passo

1. **Passo 1 — Máximo**
   - Deixe o **acelerador na posição máxima**.
   - **Mantenha por 3 segundos** sem soltar.
   - Ao final dos 3 segundos o buzzer emite **três beeps curtos** — o máximo foi memorizado.

2. **Passo 2 — Mínimo**
   - Leve o **acelerador à posição mínima**.
   - **Mantenha por 3 segundos** sem mexer.
   - Ao final dos 3 segundos o buzzer toca uma **melodia de calibração concluída** — a calibração terminou.

Se você soltar o acelerador antes dos 3 segundos em qualquer passo, esse passo é cancelado. Basta repetir: segure de novo na posição correta por 3 segundos.

---

## 3. Armar e desarmar

- **Armar** = liberar o envio de potência ao motor (o acelerador passa a comandar o ESC).
- **Desarmar** = cortar a potência ao motor; o sistema fica em modo seguro.

### Como armar

1. Certifique-se de que a **calibração foi concluída**.
2. Deixe o **acelerador na posição mínima** (0%).
3. Aperte e solte **brevemente** o botão, um bip será emitido.
3. **Mantenha o botão pressionado por 2 segundos** (pressão longa).
4. Ao armar, o sistema vai emitir bips enquanto o motor estiver parado, avisando que o sistema está pronto para iniciar.

### Como desarmar

1. **Mantenha o botão pressionado por 2 segundos** (pressão longa) com o sistema já armado.
2. O buzzer toca a **melodia de desarmado**.
3. O motor deixa de receber comando; o ESC fica no mínimo (motor parado).

---

## 4. Desarme automático por falha no acelerador

O sistema desarma automaticamente, sem intervenção do piloto, se a leitura do acelerador deixar de ser confiável:

- **Acelerador com fio:** qualquer leitura fora da faixa calibrada (por exemplo, cabo do sensor Hall rompido ou solto) ou falha de comunicação com o ADS1115 desarma **imediatamente**.
- **Acelerador sem fio (remote):** perda de sinal do remote por mais de 3 segundos desarma o sistema. Entre 500 ms e 3 s sem sinal, a potência é reduzida gradualmente a zero, mas o sistema continua armado.

Em qualquer um dos dois casos:
- O buzzer toca um padrão de alarme contínuo, diferente do beep de desarme manual.
- A página de Telemetria mostra um aviso permanente na tela informando o motivo (não desaparece sozinho).
- Para voltar a voar, primeiro corrija o problema (verifique a conexão do sensor/cabo, ou o link do remote) e depois arme novamente pelo procedimento normal.

---

## 5. Não consigo armar — acelerador não está em zero

O sistema **só permite armar com o acelerador na posição mínima** (0%). É uma proteção para evitar aceleração involuntária ao armar.

- Se você tentar armar **acelerado** (qualquer valor acima de zero), o sistema **não arma** e o buzzer toca um **alerta sonoro** (três tons repetidos).
- **O que fazer:** solte o botão, **solte o acelerador por completo**, espere um instante e tente armar de novo.

**Atenção:** Se você tentar armar com ele acelerado **mais de três vezes seguidas**, o sistema **apaga a calibração** do acelerador. Será necessário **recalibrar** (seção 2) e depois armar com o acelerador em zero.

---

## 6. Alerta quando armado e motor parado

Se o sistema estiver **armado** e o **motor estiver parado** (acelerador em zero), o buzzer emite um **alerta contínuo** (dois tons alternados). Isso avisa que o equipamento está pronto para acelerar e que o motor está parado.

- O alerta **para** quando você acelera (motor em funcionamento) ou quando **desarma** o sistema.

---

## 7. Controle de potência pelos sensores

O controlador **reduz automaticamente a potência máxima** enviada ao motor com base em três fontes: **tensão da bateria**, **temperatura do motor** e **temperatura do ESC**. O limite efetivo é o **mais restritivo** entre os três (o que permitir menor potência “ganha”).

### 7.1 Bateria (tensão baixa)

- Quando a **tensão da bateria** fica **abaixo do mínimo configurado**, o sistema começa a **reduzir a potência** de forma progressiva.
- Objetivo: proteger a bateria contra descarga excessiva e alongar a vida útil.
- Ao **armar**, o “piso” de potência da bateria é reajustado (permite uso normal até que a tensão caia de novo).

### 7.2 Temperatura do motor

- O controlador lê a temperatura do motor (por exemplo, via sensor NTC).
- Existe uma **temperatura de início de redução** e uma **temperatura máxima** (definidas nas configurações).
- Entre essa faixa, a **potência é reduzida gradualmente** (quanto mais quente, menos potência permitida).
- Objetivo: evitar superaquecimento do motor.

### 7.3 Temperatura do ESC

- A temperatura do ESC é lida via barramento CAN (em builds T-Motor) ou por sensor analógico (em builds como XAG).
- Assim como no motor, há **temperatura de início de redução** e **temperatura máxima**.
- Entre essa faixa, a **potência é reduzida gradualmente**.
- Objetivo: proteger o ESC contra sobrecarga térmica.

### Comportamento geral

- Enquanto bateria, motor e ESC estiverem dentro dos limites configurados, a **potência máxima permitida é 100%** (limitada apenas pela posição do acelerador).
- Quando qualquer um dos três entra na faixa de proteção, o sistema **limita o teto de potência**; você pode abrir o acelerador todo, mas o valor enviado ao ESC será limitado pelo sensor que estiver mais restritivo.
- Os pontos exatos de tensão mínima e temperaturas podem ser configurados (por exemplo, via servidor web ou configurações do firmware, conforme sua versão do Fly Controller).

### Sensor inválido ou perdido

Se a leitura de um sensor (temperatura do motor, temperatura do ESC ou tensão da bateria) for fisicamente impossível (por exemplo, sensor desconectado ou falha de comunicação), o sistema trata isso de duas formas, dependendo de quando o problema aparece:

- **Sensor já inválido no momento em que você arma:** a proteção daquele sensor específico fica desabilitada durante todo o voo — a potência não é limitada por ele, mesmo que a leitura volte ao normal depois. A tela mostra um aviso permanente indicando qual sensor está com problema.
- **Sensor válido ao armar, e fica inválido durante o voo:** se a leitura continuar inválida por **2 segundos seguidos**, o sistema **desarma automaticamente**, com o mesmo alarme sonoro contínuo e aviso permanente na tela usados para falha do acelerador. Falhas mais curtas que isso são ignoradas — uma piscada momentânea na comunicação é normal e não justifica cortar o motor. Uma temperatura ou tensão não muda de forma perigosa nesses 2 segundos, então essa tolerância não abre mão de nenhuma proteção real. Isso porque uma proteção que você esperava estar ativa deixou de funcionar no meio do voo, sem aviso — o sistema prefere parar o motor a continuar voando com uma proteção que parou de existir silenciosamente.

Em ambos os casos, para voltar a operar normalmente, corrija o problema do sensor e desarme/arme novamente.

---

*Fly Controller — Controlador de voo paramotores elétricos*
*Documentação em português do Brasil.*
