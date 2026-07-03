#ifndef PINS_H
#define PINS_H

/*
 * ============================================================
 *  MAPA DE PINOS  --  Armazenamento de energia por gravidade
 *  Placa: Raspberry Pi Pico 2 (RP2350)
 * ============================================================
 *
 *  Ajuste os pinos conforme a montagem real. Todos os limiares
 *  e razoes de calibracao ficam concentrados aqui.
 */

#include "hardware/i2c.h"
#include "hardware/uart.h"

/* ---------------- Sensor de distancia HC-SR04 ---------------- */
/* ATENCAO: o ECHO do HC-SR04 e 5V -> usar divisor/level-shifter para 3V3. */
#define HCSR04_TRIG_PIN     17
#define HCSR04_ECHO_PIN     16
#define HCSR04_TIMEOUT_US   30000   /* ~5 m de alcance maximo antes de desistir */

/* ---------------- MPU6050 (I2C) ---------------- */
#define MPU_I2C_PORT        i2c0
#define MPU_SDA_PIN         4
#define MPU_SCL_PIN         5
#define MPU_I2C_BAUD        400000  /* 400 kHz fast mode */

/* ---------------- Freio eletromecanico (MOSFET) ---------------- */
/* Nivel logico alto no gate -> freio ENGATADO (curto no motor). */
#define BRAKE_MOSFET_PIN    15
#define BRAKE_ACTIVE_LEVEL  1       /* 1 = engata em nivel alto */

/*
 * Reservado para expansao futura (fora do escopo atual):
 * rele que controla a entrada da fonte de 24V do motor na subida.
 * Basta definir o pino e implementar actuator_set_motor_relay().
 */
/* #define MOTOR_RELAY_PIN  14 */

/* ---------------- Botoes de comando manual ---------------- */
/* Ligados ao GND com pull-up interno -> pressionado = nivel 0. */
#define BTN_UP_PIN          10
#define BTN_DOWN_PIN        11
#define BTN_STOP_PIN        12

/* ---------------- LEDs de status (opcional) ---------------- */
#define LED_MODE_PIN        18   /* pisca/acende conforme o modo */
#define LED_BRAKE_PIN       19   /* aceso quando o freio esta engatado */

/* ---------------- Bluetooth (modulo HC-06 via UART) ---------------- */
/* O driver hc06 (src/drivers/hc06.c) auto-configura nome/PIN/baud por AT no boot.
 * O debug (printf) sai pela USB, entao a UART fica dedicada ao HC-06.
 * OBS: o guia do curso usa uart1/GP4-GP5, mas aqui GP4/GP5 sao o I2C do MPU;
 * por isso o HC-06 fica no uart0/GP0-GP1. */
#define HC06_UART_ID        uart0
#define HC06_BAUD_RATE      115200
#define HC06_TX_PIN         0       /* Pico TX -> RX do HC-06 */
#define HC06_RX_PIN         1       /* Pico RX <- TX do HC-06 (nivel 3V3) */
#define HC06_ENABLE_PIN     2       /* EN/KEY (opcional p/ modo AT); livre */
#define HC06_STATE_PIN      3       /* STATE: alto quando conectado (entrada) */

/* Identidade do modulo, gravada por AT no primeiro boot. */
#define HC06_NAME           "GRAVITY-BT"
#define HC06_PIN            "1234"

/* ---------------- Medida de potencia (ADC) ---------------- */
/*
 * ADC da Pico: 0..3V3, 12 bits (0..4095).
 * VOLTAGE  -> divisor resistivo (bloco -> ADC).
 * CURRENT  -> ACS712/shunt. O ACS712 e alimentado em 5V; sua saida (centro
 *             em 2V5, ate ~5V) PRECISA de divisor proprio para nao passar
 *             de 3V3 no ADC.
 */
#define ADC_VOLTAGE_PIN     26      /* GP26 = ADC0 */
#define ADC_VOLTAGE_CHANNEL 0
#define ADC_CURRENT_PIN     27      /* GP27 = ADC1 */
#define ADC_CURRENT_CHANNEL 1

#define ADC_VREF            3.3f
#define ADC_MAX_COUNTS      4095.0f

/*
 * ---- Constantes de calibracao (AJUSTAR com a montagem real) ----
 * Definir a partir da tensao/corrente MAXIMAS esperadas do gerador.
 */

/* Tensao: Vbloco = Vadc * VOLTAGE_DIVIDER_RATIO.
 * Ex.: divisor 100k(topo)/10k(baixo) -> ratio ~11.0 (le ate ~36V). */
#define VOLTAGE_DIVIDER_RATIO   11.0f

/* Corrente (ACS712): Vout = ACS712_ZERO_V + I * ACS712_SENS_V_PER_A.
 * O nivel visto no ADC ja passa pelo divisor de saida do ACS712:
 *   Vadc = Vout_acs * ACS712_OUT_DIVIDER
 * Modelos tipicos: 5A -> 0.185 V/A, 20A -> 0.100 V/A, 30A -> 0.066 V/A. */
#define ACS712_ZERO_V           2.5f    /* tensao de saida em 0 A (Vcc/2) */
#define ACS712_SENS_V_PER_A     0.100f  /* sensibilidade do modelo usado */
#define ACS712_OUT_DIVIDER      0.66f   /* divisor da saida 5V do ACS712 -> ADC */

#endif /* PINS_H */
