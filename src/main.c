/*
 * Armazenamento de energia por gravidade -- firmware FreeRTOS (Pico 2 / RP2350).
 *
 * Arquitetura: single-core, tasks + ISRs. Ver plano do projeto.
 *   control_task    -> maquina de estados + freio (unico atuador no escopo atual)
 *   imu_task        -> MPU6050: inclinacao, 2a velocidade, alarme de balanco
 *   sensor_distance -> HC-SR04: distancia + velocidade
 *   power_task      -> tensao/corrente na descida (ADC)
 *   command_task    -> botoes + UART do HC-06 (Bluetooth)
 *   telemetry_task  -> CSV pela UART do HC-06
 *
 * Debug (printf) sai pela USB; a UART fica dedicada ao HC-06.
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#include "pins.h"
#include "app_state.h"
#include "tasks.h"

#include "actuator.h"
#include "power_sense.h"
#include "hcsr04.h"
#include "hc06.h"

/* ---------------- Handles globais (declarados extern em app_state.h) ---------------- */
QueueHandle_t      xQueueCommands  = NULL;
QueueHandle_t      xQueueEvents    = NULL;
QueueHandle_t      xQueueTelemetry = NULL;
SemaphoreHandle_t  xMutexI2C       = NULL;
SemaphoreHandle_t  xMutexADC       = NULL;
EventGroupHandle_t xEventGroupMode = NULL;

static void fatal_error_loop(const char *msg) {
    while (1) {
        printf("[FATAL] %s\n", msg);
        sleep_ms(1000);
    }
}

static void hc06_uart_init(void) {
    uart_init(HC06_UART_ID, HC06_BAUD_RATE);
    gpio_set_function(HC06_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(HC06_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(HC06_UART_ID, false, false);   /* sem CTS/RTS */
    uart_set_format(HC06_UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(HC06_UART_ID, true);

    /* STATE: alto quando o modulo esta conectado (so leitura/status). */
    gpio_init(HC06_STATE_PIN);
    gpio_set_dir(HC06_STATE_PIN, GPIO_IN);
}

int main(void) {
    stdio_init_all();        /* USB CDC para debug */
    sleep_ms(1000);
    printf("\n=== Gravity Energy Storage - boot ===\n");

    /* --- Perifericos (nao dependem do scheduler) --- */
    hc06_uart_init();
    /* Auto-configura o HC-06 por AT (nome/PIN/115200). Roda antes do scheduler;
     * usa sleeps bloqueantes, o que aqui e seguro. Se o modulo nao responder,
     * segue mesmo assim (ex.: ja configurado). */
    hc06_config(HC06_NAME, HC06_PIN);
    actuator_init();         /* freio no estado seguro (solto) */
    power_sense_init();      /* ADC tensao/corrente */
    hcsr04_init();           /* cria semaforo interno + IRQ do eco */
    /* mpu6050_init() e chamado dentro da imu_task, sob o mutex de I2C. */

    /* --- IPC --- */
    xQueueCommands  = xQueueCreate(16, sizeof(command_t));
    xQueueEvents    = xQueueCreate(8,  sizeof(event_t));
    xQueueTelemetry = xQueueCreate(64, sizeof(telemetry_sample_t));
    xMutexI2C       = xSemaphoreCreateMutex();
    xMutexADC       = xSemaphoreCreateMutex();
    xEventGroupMode = xEventGroupCreate();

    if (!xQueueCommands || !xQueueEvents || !xQueueTelemetry ||
        !xMutexI2C || !xMutexADC || !xEventGroupMode) {
        fatal_error_loop("falha ao criar objetos de IPC");
    }

    /* --- Tasks (prioridade maior = mais urgente) --- */
    BaseType_t ok = pdPASS;
    ok &= xTaskCreate(control_task,        "control", 1024, NULL, 4, NULL);
    ok &= xTaskCreate(imu_task,            "imu",     1024, NULL, 3, NULL);
    ok &= xTaskCreate(sensor_distance_task,"dist",    1024, NULL, 2, NULL);
    ok &= xTaskCreate(power_task,          "power",   1024, NULL, 2, NULL);
    ok &= xTaskCreate(command_task,        "cmd",     1024, NULL, 2, NULL);
    ok &= xTaskCreate(telemetry_task,      "tlm",     1024, NULL, 1, NULL);

    if (ok != pdPASS) {
        fatal_error_loop("falha ao criar tasks");
    }

    vTaskStartScheduler();

    /* Nao deveria chegar aqui. */
    for (;;);
    return 0;
}
