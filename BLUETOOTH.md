# Bluetooth (HC-06) — passo a passo e revisão

Como o módulo **HC-06** é ligado, configurado e usado neste projeto para enviar a
telemetria (CSV) e receber comandos. Baseado no material do curso
(`hc06.h`/`hc06_config`) e reconciliado com o firmware deste repositório.

---

## Como funciona aqui (visão geral)

- O HC-06 é um **Bluetooth serial transparente**: para o PC ele vira uma porta serial
  (COM no Windows, `/dev/rfcomm0` no Linux). Tudo que a Pico escreve na UART chega no
  PC e vice-versa — o "Bluetooth" acontece dentro do módulo.
- A Pico fala com o HC-06 por **UART de hardware** (`uart0`, GP0/GP1). O **debug**
  (`printf`, mensagens `[CTRL]`) sai pela **USB**, separado da telemetria.
- No boot, o firmware chama **`hc06_config(HC06_NAME, HC06_PIN)`**
  ([src/drivers/hc06.c](src/drivers/hc06.c)), que via comandos **AT**:
  1. detecta o baud atual do módulo (tenta 9600 e depois 115200);
  2. sobe o módulo para **115200**;
  3. grava o **nome** (`GRAVITY-BT`) e o **PIN** (`1234`).
  Isso roda **antes** do scheduler do FreeRTOS, então pode usar `sleep_ms` sem problema.
- Depois disso, [telemetry_task](src/tasks/telemetry_task.c) escreve as linhas CSV e
  [command_task](src/tasks/command_task.c) lê os comandos de texto — os dois na mesma
  UART (TX e RX são independentes no hardware, sem conflito).

Nome/PIN/baud ficam em [src/pins.h](src/pins.h) (`HC06_NAME`, `HC06_PIN`, `HC06_BAUD_RATE`).

---

## Revisão da conexão (o que foi ajustado)

Comparando o firmware com o material de referência, os pontos abaixo foram
reconciliados:

1. **Auto-configuração por AT.** Antes o firmware assumia o módulo já em 115200. Agora
   ele vendoriza o driver do curso e chama `hc06_config()` no boot, que resolve o baud
   sozinho (o HC-06 de fábrica vem em **9600**) e grava nome/PIN. Mais robusto.
2. **Conflito de pinos evitado.** O guia do curso usa **uart1/GP4-GP5** para o HC-06,
   mas aqui **GP4/GP5 são o I2C do MPU6050**. Por isso o HC-06 ficou no **uart0/GP0-GP1**.
   Só mudar em [src/pins.h](src/pins.h) se necessário.
3. **Init da UART mais completa.** `hc06_uart_init()` agora desliga controle de fluxo
   (`uart_set_hw_flow(false,false)`) e fixa o formato **8N1** (`uart_set_format`), como
   no exemplo do curso.
4. **Pino STATE.** GP3 é lido como entrada para status de conexão (alto = conectado).
   Disponível para acender um LED "conectado" ou para o PC sinalizar presença.
5. **RX por polling (e não IRQ).** O exemplo do curso usa IRQ de UART para o RX. Aqui os
   comandos são texto de baixa taxa, então `command_task` faz **polling** com a FIFO
   ligada — mais simples e suficiente. Se um dia precisar de RX pesado, dá para migrar
   para IRQ como no exemplo (`uart_set_irq_enables` + `xQueueSendFromISR`).

> **HC-05 x HC-06.** O driver aqui é para **HC-06** (comandos AT sem `\r\n`, `AT+BAUD8`,
> `AT+NAME`, `AT+PIN`). Um **HC-05** usa outro conjunto de AT e entra em modo de
> configuração segurando o pino EN/KEY em nível alto no power-on (AT a 38400). Se o seu
> módulo for HC-05, troque o driver ou configure-o à parte; a fiação de dados (TX/RX) é
> a mesma.

---

## Ligações

| HC-06 | Pico | Observação |
|-------|------|-----------|
| VCC | 5 V (VBUS) | alimentação |
| GND | GND | **terra comum** com a Pico |
| RX | **GP0** (Pico TX) | Pico 3,3 V → RX do HC-06 (ok direto) |
| TX | **GP1** (Pico RX) | TX do HC-06 é 3,3 V → GP1 |
| STATE | **GP3** | opcional: alto quando conectado |
| EN/KEY | **GP2** | opcional: só p/ forçar modo AT |

```
   Pico                         HC-06
   GP0 (TX) ───────────────►  RX
   GP1 (RX) ◄───────────────  TX
   GND ─────────────────────  GND
   5V  ─────────────────────  VCC
   GP3 ◄──────────────────── STATE   (opcional)
   GP2 ──────────────────►   EN      (opcional)
```

---

## Passo 1 — Gravar e ver a configuração pela USB

Com o HC-06 ligado (mas **sem** parear ainda), grave o firmware e abra o monitor
serial **USB** (`screen /dev/ttyACM0 115200`). No boot deve aparecer:

```
=== Gravity Energy Storage - boot ===
Tentando baud = 9600...
Conectado!

Alterando baud rate para 115200...
Baud rate OK

Configurando nome...
Nome OK

Configurando PIN...
PIN OK

HC-06 configurado!
[CTRL] modo -> IDLE
```

Se aparecer `ERRO: HC-06 nao respondeu`, veja a tabela de problemas no fim. O firmware
segue funcionando mesmo assim (útil quando o módulo já estava configurado).

---

## Passo 2 — Parear com o PC

### Linux (Ubuntu)
```bash
# 1) descobrir o MAC do modulo (nome GRAVITY-BT)
bluetoothctl
  scan on            # anote o MAC (ex.: 98:D3:31:XX:XX:XX)
  pair 98:D3:31:XX:XX:XX     # PIN: 1234
  trust 98:D3:31:XX:XX:XX
  quit

# 2) criar a porta serial rfcomm
sudo rfcomm bind 0 98:D3:31:XX:XX:XX
# -> cria /dev/rfcomm0
```

### Windows
1. Configurações → Bluetooth → adicionar dispositivo → **GRAVITY-BT**, PIN **1234**.
2. O Windows cria uma porta **COM de saída** (Gerenciador de Dispositivos → Portas COM).
   Use essa COM no script.

> **Pareado ≠ Conectado.** Parear salva as credenciais; a conexão (dados) só abre quando
> um programa abre a porta serial (`/dev/rfcomm0` ou a COM). O LED do HC-06 para de
> piscar quando conecta.

---

## Passo 3 — Rodar o script de telemetria

```bash
pip install -r tools/requirements.txt

# ao vivo: plota e loga; digite comandos no teclado (up/down/stop/brake/release/cal)
python3 tools/telemetry.py --port /dev/rfcomm0 --log sessao.csv
```

Você deve ver as linhas CSV chegando e os gráficos atualizando. Para mandar um comando
único sem abrir o gráfico: `python3 tools/telemetry.py --port /dev/rfcomm0 --send down`.

**Dica de bancada:** enquanto o HC-06 não está pronto, `TELEMETRY_ALSO_USB=1` (padrão)
faz a mesma telemetria sair pela USB — aponte o script para `/dev/ttyACM0` e teste tudo
igual, sem Bluetooth.

---

## Passo 4 — Comandos aceitos

Texto (linha + Enter), tanto pela USB quanto pelo Bluetooth:

| Comando | Efeito |
|---------|--------|
| `up` / `subir` | modo SUBINDO |
| `down` / `descer` | modo DESCENDO (liga a medida de potência) |
| `stop` / `parar` | modo IDLE |
| `brake` / `freio` | engata o freio (modo FREADO) |
| `release` / `solta` | solta o freio (modo IDLE) |
| `cal` | zera o offset do sensor de corrente |

---

## Resolução de problemas

| Sintoma | Causa provável | O que fazer |
|---------|----------------|-------------|
| `HC-06 nao respondeu` no boot | TX/RX trocados; sem GND comum; módulo já pareado/conectado (não aceita AT enquanto conectado) | Cheque fiação, desconecte do PC e re-energize; teste com o módulo desconectado |
| Config OK, mas nada chega no PC | porta serial errada; ainda não "conectou" | Confirme `/dev/rfcomm0`/COM; abra a porta (o script abre); veja o LED do HC-06 parar de piscar |
| Lixo/caracteres errados no PC | baud diferente | O firmware deixa o módulo em **115200**; use 115200 no script (`--baud`) |
| Não acha o módulo no scan | sem alimentação; já conectado a outro host | Confirme 5 V/GND; desconecte de outros dispositivos |
| Comandos ignorados | faltou o `\n`; digitou no gráfico em vez do terminal | Envie linha + Enter; use `--send` ou o campo de teclado do script |
| Quer trocar nome/PIN | — | Edite `HC06_NAME`/`HC06_PIN` em [src/pins.h](src/pins.h) e regrave (reconfigura no próximo boot) |

---

## Ajustes rápidos

- **Trocar a UART/pinos:** `HC06_UART_ID`, `HC06_TX_PIN`, `HC06_RX_PIN` em [src/pins.h](src/pins.h).
- **Nome/PIN/baud:** `HC06_NAME`, `HC06_PIN`, `HC06_BAUD_RATE` em [src/pins.h](src/pins.h).
- **Parar de espelhar telemetria na USB:** defina `TELEMETRY_ALSO_USB 0` em
  [src/tasks/telemetry_task.c](src/tasks/telemetry_task.c).
