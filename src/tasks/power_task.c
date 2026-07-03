#include "tasks.h"
#include "app_state.h"
#include "power_sense.h"

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

/*
 * Mede tensao e corrente do circuito APENAS durante a descida (gerador).
 * Fica bloqueada em xEventGroupWaitBits ate a control_task setar MODE_BIT_DOWN,
 * evitando consumir CPU/ADC quando nao ha o que medir.
 */
void power_task(void *p) {
    (void) p;

    for (;;) {
        /* Espera o modo DESCENDO (nao limpa o bit: outros tambem o leem). */
        xEventGroupWaitBits(xEventGroupMode, MODE_BIT_DOWN,
                            pdFALSE, pdFALSE, portMAX_DELAY);

        float voltage = 0.0f, current = 0.0f;
        if (xSemaphoreTake(xMutexADC, portMAX_DELAY) == pdTRUE) {
            voltage = power_sense_read_voltage();
            current = power_sense_read_current();
            xSemaphoreGive(xMutexADC);
        }

        float power = voltage * current;
        telemetry_push(TLM_POWER,
                       xTaskGetTickCount() * (1000 / configTICK_RATE_HZ),
                       voltage, current, power);

        vTaskDelay(pdMS_TO_TICKS(POWER_SAMPLE_MS));
    }
}
