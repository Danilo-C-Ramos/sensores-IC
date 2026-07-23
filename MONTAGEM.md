# Montagem e testes incrementais

Guia passo a passo para montar o circuito **por partes** e testar cada subsistema
isoladamente antes de integrar tudo. A ordem é pensada para você validar a lógica
com LEDs/multímetro **antes** de encostar na parte de potência (motor 24 V / gerador).

> ⚠️ **Segurança primeiro.** A parte de potência (motor a 24 V, gerador, freio por
> curto no motor) tem energia suficiente para causar dano. Monte e valide toda a
> lógica de baixa tensão (etapas 0–4) com LEDs de teste. Só conecte no motor real
> depois que a lógica estiver 100 %. Nunca ligue nada acima de **3,3 V** direto num
> pino da Pico — sempre confira com multímetro antes.

---

## Convenções

- Placa: **Raspberry Pi Pico 2 (RP2350)**. Os números `GPxx` são os GPIOs.
- **Terra comum obrigatório:** todos os GNDs (Pico, HC-SR04, HC-06, ACS712, fonte
  24 V, motor) devem estar no mesmo nó de GND. Sem isso, nada funciona.
- Toda a pinagem está centralizada em [`src/pins.h`](src/pins.h) — se mudar um pino
  no hardware, mude **só** lá e recompile.
- Dois canais seriais independentes:
  - **USB (debug):** mensagens `[CTRL] ...`, banner de boot, alarmes — via `printf`.
  - **UART do HC-06 (telemetria):** as linhas CSV `D/I/P/S`.
  - Com `TELEMETRY_ALSO_USB=1` (padrão), **a telemetria também sai na USB**, então
    dá para testar tudo pelo monitor serial USB antes de ter o HC-06. Coloque `0`
    em produção.

### Mapa de pinos (resumo)

| Subsistema | Sinal | Pino | Observação |
|-----------|-------|------|-----------|
| HC-SR04 | TRIG | GP17 | saída 3,3 V (ok direto) |
| HC-SR04 | ECHO | GP16 | **5 V → divisor para 3,3 V** |
| MPU6050 | SDA / SCL | GP4 / GP5 | I2C0, 400 kHz, addr 0x68 |
| Freio | gate MOSFET | GP15 | nível alto = freio ENGATADO |
| Botões | UP / DOWN / STOP | GP10 / GP11 / GP12 | pull-up interno, pressionado = 0 |
| LEDs | MODO / FREIO | GP18 / GP19 | com resistor ~330 Ω p/ GND |
| Tensão | ADC0 | GP26 | divisor resistivo |
| Corrente | ADC1 | GP27 | ACS712 (**saída 5 V → divisor**) |
| HC-06 | Pico TX → HC-06 RX | GP0 | UART0 |
| HC-06 | Pico RX ← HC-06 TX | GP1 | UART0 |
| HC-06 | EN / STATE | GP2 / GP3 | opcionais |
| (futuro) | relé da fonte 24 V | GP14 | fora do escopo atual |

### Material sugerido

- Protoboard + jumpers, multímetro.
- 3 botões, 2 LEDs + resistores ~330 Ω.
- HC-SR04, MPU6050 (GY-521), HC-06.
- MOSFET N-canal **logic-level** (ex.: IRLZ44N / AO3400 conforme a corrente),
  resistor de gate ~220 Ω e pull-down de gate ~10 kΩ.
- ACS712 (5 A / 20 A / 30 A conforme sua corrente), resistores para os divisores.
- Fonte 24 V, motor DC, o LED/carga da descida.

---

## Como compilar e gravar

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_MAKE_PROGRAM=$HOME/.pico-sdk/ninja/v1.12.1/ninja
cmake --build build
```

Gera `build/pico_emb.uf2`. Para gravar: segure **BOOTSEL**, conecte o USB, solte, e
copie o `.uf2` para o drive `RP2350` que aparece. (Ou use `picotool load -f build/pico_emb.uf2`.)

Monitor serial USB (Linux): `screen /dev/ttyACM0 115200` ou `minicom -D /dev/ttyACM0`.

---

## Etapa 0 — Bring-up (só Pico + USB)

**Objetivo:** confirmar que o firmware sobe e o FreeRTOS inicia.

1. Só a Pico no USB, nada mais ligado.
2. Grave o `.uf2` e abra o monitor serial USB.

**Esperado:**
```
=== Gravity Energy Storage - boot ===
[CTRL] modo -> IDLE
```
Se aparecer, o scheduler e as tasks estão rodando. As leituras de sensor vão falhar
em silêncio (nada ligado) — normal por enquanto.

**Problemas comuns:** nada na serial → confira que gravou o `.uf2` certo e que está
lendo `/dev/ttyACM*` a 115200.

---

## Etapa 1 — Sensor de distância HC-SR04

**Objetivo:** medir distância e velocidade do bloco.

**Ligações:**
- **VCC** do HC-SR04 → **5 V** (VBUS/VSYS), **GND** → GND.
- **TRIG** → **GP17** (saída 3,3 V da Pico serve).
- **ECHO** (5 V!) → **divisor resistivo** → **GP16**. Ex.: ECHO —[1 kΩ]—•—[2 kΩ]— GND,
  com o ponto do meio (•) indo pra GP16 → `5 V · 2/(1+2) ≈ 3,3 V`.

**Teste (monitor USB):** aparecem linhas `D,tempo,distancia_cm,velocidade`:
```
D,1234,42.5,0.0
D,1294,38.0,-75.0
```
Aproxime/afaste um alvo (parede/mão) e veja `distancia_cm` mudar e a velocidade
acompanhar o movimento.

**Problemas comuns:** nenhuma linha `D` → sem eco (alvo fora de alcance, TRIG/ECHO
trocados, ou o divisor no ECHO errado). Valores absurdos → confira o divisor e o GND.

---

## Etapa 2 — MPU6050 (I2C)

**Objetivo:** inclinação (balanço), 2ª velocidade e o alarme que engata o freio.

**Ligações:**
- **VCC** do módulo GY-521 → 3,3 V (a maioria aceita 3,3–5 V; os pinos SDA/SCL são 3,3 V).
- **GND** → GND.
- **SDA** → **GP4**, **SCL** → **GP5** (o módulo já traz pull-ups).
- **AD0** → GND (mantém o endereço em **0x68**).

**Teste (monitor USB):** linhas `I,tempo,vel_imu,inclinacao,accel_g`:
```
I,1010,0.0,1.2,1.00
```
Incline o sensor e veja `inclinacao` subir. Passando de **20°** (`SWAY_TILT_ALARM_DEG`):
```
[CTRL] ALARME balanco 21.3 deg -> freio ON
```
o modo vai para `FREADO` — é o requisito de travar por balanço. (Se você já montou o
LED de FREIO da etapa 9, ele acende junto; senão, confira pela mensagem na serial.)

**Problemas comuns:** **nenhuma** linha `I` → o `mpu6050_init()` não achou o sensor
(WHOAMI ≠ 0x68). Cheque SDA/SCL (não invertidos), alimentação e o AD0. Ajuste o
limiar em [`src/tasks/tasks.h`](src/tasks/tasks.h) se precisar.

---

## Etapa 3 — Freio (MOSFET) — teste com LED antes do motor!

**Objetivo:** validar o acionamento do freio **sem** risco. Use um LED como "motor
fake" para enxergar o freio engatando.

**Ligações (fase de teste, com LED):**
- **GP15** → resistor 220 Ω → **gate** do MOSFET.
- **gate → GND** com resistor 10 kΩ (pull-down: mantém o freio solto se o pino ficar
  flutuando durante o boot).
- **source** → GND.
- **drain** → LED "motor fake" → resistor 330 Ω → 3,3 V.

**Teste (monitor USB, mande comandos por texto — ver a *Referência rápida de
comandos* no fim do guia):**
- No boot o freio está **solto** (LED do dreno apagado) — estado seguro.
- Comando `brake` → `[CTRL] modo -> FREADO (freio ON)` e o "motor fake" é chaveado
  (LED do dreno acende). Se já montou o LED de FREIO (GP19) da etapa 9, ele acende junto.
- Comando `release` → freio solta.

**Só depois de validar:** troque o LED fake pelos **terminais do motor** (o MOSFET dá
curto entre eles = freio dinâmico). Dimensione o MOSFET para a corrente de curto do
motor e mantenha o **GND comum** com a Pico.

> ⚠️ Freio dinâmico (curto no motor) freia pela geração — ele **resiste** ao
> movimento, não segura um peso parado indefinidamente. Considere um freio mecânico
> se precisar segurar estático.

---

## Etapa 4 — Bluetooth (HC-06) + script Python

**Objetivo:** telemetria e comandos sem fio.

**Ligações (uart0):**
- **VCC** do HC-06 → 5 V, **GND** → GND.
- **Pico TX (GP0)** → **RX do HC-06**.
- **TX do HC-06** → **Pico RX (GP1)**.
- **STATE** → **GP3** (opcional; status de conexão).
- **EN/KEY** → **GP2** (opcional; só usado se for entrar em modo AT).

O firmware **auto-configura** o HC-06 no boot (nome, PIN e baud 115200) via `hc06_config()`
— não precisa mexer no baud à mão. Veja o passo a passo completo, pareamento e
resolução de problemas em **[BLUETOOTH.md](BLUETOOTH.md)**.

**Dica:** com `TELEMETRY_ALSO_USB=1` você pode usar o mesmo script apontando para a
**USB** (`--port /dev/ttyACM0`) antes do HC-06 estar pronto.

---

## Etapa 5 — Medida de tensão (divisor no ADC)

**Objetivo:** ler a tensão do circuito na descida.

> A tensão é medida **só no modo DESCENDO** — mande o comando `down` para habilitar
> a `power_task`.

**Ligações:**
- Divisor da tensão do bloco/gerador para **GP26**: `V_bloco —[R1]—•—[R2]— GND`,
  ponto do meio → GP26. Escolha R1/R2 para que, na tensão **máxima**, o ponto médio
  fique **abaixo de 3,3 V**. Ex.: R1 = 100 kΩ, R2 = 10 kΩ → razão ≈ 11 (lê até ~36 V).
- A relação está em [`src/pins.h`](src/pins.h): `VOLTAGE_DIVIDER_RATIO`.

**Calibração:**
1. **Com multímetro**, confirme que o ponto médio do divisor **não passa de 3,3 V** na
   tensão máxima — só então ligue em GP26.
2. Aplique uma tensão conhecida, mande `down`, olhe a coluna de tensão das linhas `P`.
3. Ajuste `VOLTAGE_DIVIDER_RATIO` até bater com o multímetro e recompile.

**Esperado:** `P,tempo,tensao_V,corrente_A,potencia_W` com a tensão correta.

---

## Etapa 6 — Medida de corrente (ACS712)

**Objetivo:** ler a corrente do circuito na descida.

**Ligações:**
- O ACS712 fica **em série** com a carga (os dois terminais de potência).
- Alimente o ACS712 com **5 V** e GND (comum).
- A saída do ACS712 é centrada em **2,5 V** e pode chegar a ~5 V → passe por um
  **divisor** até **GP27** (`ACS712_OUT_DIVIDER`, ex.: 0,66 → ~3,3 V no fundo de escala).

**Calibração:**
1. Sem corrente na carga, mande o comando **`cal`** → zera o offset do sensor
   (`[CTRL] calibracao de corrente (zero) feita`).
2. Ajuste `ACS712_SENS_V_PER_A` conforme o modelo (5 A→0,185; 20 A→0,100; 30 A→0,066)
   e o `ACS712_OUT_DIVIDER` conforme seu divisor. Recompile.
3. Passe uma corrente conhecida e compare com a coluna de corrente das linhas `P`.

---

## Etapa 7 — Integração e ciclo completo

Com tudo montado e validado individualmente:

1. `up` → sobe: acompanhe `D` (velocidade positiva/negativa conforme a montagem) e `I`.
2. `stop` no topo (peso seguro).
3. `down` → desce: além de `D`/`I`, começam as linhas `P` (tensão/corrente/potência).
4. Provoque um balanço no sensor → deve vir `[CTRL] ALARME ... -> freio ON` e o modo
   ir para `FREADO`.
5. No `--replay`, confira as duas velocidades (distância × IMU) e a energia na descida.

**Checklist final:**
- [ ] GND comum entre Pico, sensores, HC-06 e potência.
- [ ] Nenhum pino da Pico acima de 3,3 V (ECHO e ACS712 com divisor conferidos no multímetro).
- [ ] Freio no estado **solto** ao ligar; engata por comando e por balanço.
- [ ] `VOLTAGE_DIVIDER_RATIO`, `ACS712_SENS_V_PER_A` e `ACS712_OUT_DIVIDER` calibrados.
- [ ] HC-06 auto-configurado no boot (ver mensagens de config na USB).

---

## Etapa 8 — Alimentar por bateria (rodar sem o cabo USB)

**Objetivo:** deixar a Pico rodando sozinha, alimentada por bateria, acompanhando tudo
pelo Bluetooth — sem PC conectado.

### O código já roda sozinho
O firmware fica gravado na **flash** da Pico. Depois de gravar **uma vez** via USB
(BOOTSEL → copiar o `.uf2`), ele **inicia automaticamente toda vez que a Pico é
energizada** — não precisa de PC. Como a telemetria também sai pelo **HC-06**, você
acompanha os dados por Bluetooth (script `telemetry.py`) sem cabo nenhum.

> O firmware **não trava esperando USB**: se não houver PC, os `printf` de debug são
> simplesmente descartados e o sistema roda normal. O freio continua **solto** no boot
> (estado seguro).

**Fluxo:**
1. Grave o `.uf2` pelo USB (BOOTSEL) — só nessa hora precisa do cabo.
2. Desconecte o USB.
3. Ligue a bateria (ver abaixo). O código sobe sozinho, configura o HC-06 e começa a
   enviar telemetria.
4. Pareie o PC ao `GRAVITY-BT` e rode `python3 tools/telemetry.py --port /dev/rfcomm0`.

### Onde ligar a bateria — pino VSYS
A Pico 2 tem um **regulador interno (buck-boost)** que aceita **1,8 V a 5,5 V** no pino
**VSYS** e gera os 3,3 V internos. Ou seja, é só ligar a bateria no **VSYS + GND**:

```
   bateria (+) ──[chave]──►  VSYS  (pino 39)
   bateria (−) ───────────►  GND   (pino 38, ou qualquer GND)
```

- Coloque uma **chave liga/desliga** em série com o (+) da bateria.
- **NUNCA** ligue bateria no pino **3V3(OUT)** (pino 36) — esse é *saída* do regulador.
- Se quiser poder usar USB **e** bateria sem risco de uma alimentar a outra, ponha um
  **diodo Schottky** (ex.: 1N5817) em série entre a bateria e o VSYS. (Ligando só na
  bateria, sem USB junto, o diodo é opcional.)

### Qual bateria (⚠️ VSYS ≤ 5,5 V)
| Opção | Tensão | OK? |
|-------|--------|-----|
| 3× AA/AAA alcalina | ~4,5 V | ✅ boa e simples |
| 4× AA NiMH recarregável | ~4,8–5,0 V | ✅ boa (ainda alimenta os 5 V) |
| 1× LiPo / 18650 | 3,0–4,2 V | ✅ (ideal com proteção/carregador) |
| **4× AA alcalina** | **~6 V** | ❌ **acima de 5,5 V — não ligar direto** |
| **Bateria 9 V** | **9 V** | ❌ **só com regulador para ≤5,5 V antes do VSYS** |

### E os componentes de 5 V?
HC-SR04, HC-06 e ACS712 esperam ~5 V — e na bateria você **não** tem o VBUS (5 V do USB).
- Uma bateria de **~4,8–5 V (4× NiMH)** costuma alimentar esses módulos direto (HC-SR04
  funciona a partir de ~4,5 V; a maioria dos HC-06 aceita ~3,6–6 V). Ligue o "5 V" deles
  no **positivo da bateria** (o mesmo nó do VSYS).
- Com bateria mais baixa (LiPo ~3,7 V), use um **conversor boost para 5 V** para esses
  módulos.
- **Terra comum:** o GND da bateria, da Pico e dos módulos tem que ser o mesmo nó.

> Dica: dá para monitorar a própria bateria ligando o VSYS (por um divisor) num canal de
> ADC livre, se quiser telemetria do nível de carga — fora do escopo atual.

---

## Etapa 9 — Botões + LEDs de status (bônus)

**Objetivo:** validar comando manual e os LEDs de estado, sem potência. Esta etapa é
**bônus**: todos os comandos já funcionam por texto (USB/HC-06) e todos os estados já
aparecem na serial — os botões e LEDs são só uma comodidade física.

**Ligações:**
- Botão UP entre **GP10** e **GND**; DOWN entre **GP11** e GND; STOP entre **GP12** e GND.
  (Pull-up é interno; pressionado = nível 0.)
- LED de MODO: **GP18** → resistor 330 Ω → LED → GND.
- LED de FREIO: **GP19** → resistor 330 Ω → LED → GND.

**Teste (monitor USB):**
- Botão UP → `[CTRL] modo -> SUBINDO`, LED de MODO acende.
- Botão DOWN → `[CTRL] modo -> DESCENDO`, LED de MODO acende.
- Botão STOP → `[CTRL] modo -> IDLE`, LED de MODO apaga.

**Problemas comuns:** botão não responde → verifique se está entre o GPIO e o **GND**
(não no 3,3 V). LED sempre aceso/apagado → resistor/polaridade do LED.

---

## Referência rápida de comandos

Enviados por texto (linha + Enter) pela USB ou pelo HC-06:

| Comando | Efeito |
|---------|--------|
| `up` / `subir` | modo SUBINDO |
| `down` / `descer` | modo DESCENDO (habilita medida de potência) |
| `stop` / `parar` | modo IDLE |
| `brake` / `freio` | engata o freio (modo FREADO) |
| `release` / `solta` | solta o freio (modo IDLE) |
| `cal` | zera o offset do sensor de corrente |
