#include "tasks.h"
#include "app_state.h"
#include "pins.h"
#include "actuator.h"
#include "power_sense.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include <stdio.h>

/*
 * Coracao do sistema: mantem o modo (IDLE/UP/DOWN/FAULT), e o UNICO a acionar
 * o freio. Bloqueia em um queue set para reagir com baixa latencia tanto a
 * comandos (botao/BT) quanto a eventos (alarme de balanco do MPU).
 */

static system_mode_t s_mode = MODE_IDLE;

static const char *mode_name(system_mode_t m) {
    switch (m) {
        case MODE_UP:    return "SUBINDO";
        case MODE_DOWN:  return "DESCENDO";
        case MODE_FAULT: return "FREADO";
        default:         return "IDLE";
    }
}

/* Aplica um novo estado: freio, bits de modo, LED e telemetria de estado. */
static void apply_mode(system_mode_t m) {
    s_mode = m;

    bool brake = (m == MODE_FAULT);
    actuator_set_brake(brake);

    EventBits_t bits = 0;
    if (m == MODE_UP)   bits = MODE_BIT_UP;
    if (m == MODE_DOWN) bits = MODE_BIT_DOWN;
    xEventGroupClearBits(xEventGroupMode, MODE_BIT_UP | MODE_BIT_DOWN);
    if (bits) xEventGroupSetBits(xEventGroupMode, bits);

    gpio_put(LED_MODE_PIN, (m == MODE_UP || m == MODE_DOWN) ? 1 : 0);

    printf("[CTRL] modo -> %s%s\n", mode_name(m), brake ? " (freio ON)" : "");
    telemetry_push(TLM_STATE, xTaskGetTickCount() * (1000 / configTICK_RATE_HZ),
                   (float) m, 0.0f, 0.0f);
}

static void handle_command(command_t c) {
    switch (c) {
        case CMD_UP:        apply_mode(MODE_UP);    break;
        case CMD_DOWN:      apply_mode(MODE_DOWN);  break;
        case CMD_STOP:      apply_mode(MODE_IDLE);  break;
        case CMD_BRAKE_ON:  apply_mode(MODE_FAULT); break;
        case CMD_BRAKE_OFF: apply_mode(MODE_IDLE);  break;
        case CMD_CAL:
            if (xSemaphoreTake(xMutexADC, portMAX_DELAY) == pdTRUE) {
                power_sense_calibrate_zero_current();
                xSemaphoreGive(xMutexADC);
            }
            printf("[CTRL] calibracao de corrente (zero) feita\n");
            break;
    }
}

static void handle_event(const event_t *e) {
    switch (e->type) {
        case EVT_SWAY_ALARM:
            /* Seguranca: balanco excessivo -> engata o freio imediatamente. */
            printf("[CTRL] ALARME balanco %.1f deg -> freio ON\n", e->value);
            apply_mode(MODE_FAULT);
            break;
        case EVT_SENSOR_FAULT:
            /* Falha isolada de leitura: ignora (evita trip por ruido). */
            break;
    }
}

void control_task(void *p) {
    (void) p;

    gpio_init(LED_MODE_PIN);
    gpio_set_dir(LED_MODE_PIN, GPIO_OUT);
    gpio_put(LED_MODE_PIN, 0);

    /* Queue set: bloqueia em comandos E eventos ao mesmo tempo. */
    QueueSetHandle_t set = xQueueCreateSet(16 + 8);
    xQueueAddToSet(xQueueCommands, set);
    xQueueAddToSet(xQueueEvents, set);

    apply_mode(MODE_IDLE);

    for (;;) {
        QueueSetMemberHandle_t who = xQueueSelectFromSet(set, portMAX_DELAY);

        if (who == xQueueCommands) {
            command_t c;
            if (xQueueReceive(xQueueCommands, &c, 0) == pdTRUE) {
                handle_command(c);
            }
        } else if (who == xQueueEvents) {
            event_t e;
            if (xQueueReceive(xQueueEvents, &e, 0) == pdTRUE) {
                handle_event(&e);
            }
        }
    }
}
