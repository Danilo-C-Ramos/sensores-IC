#include "tasks.h"
#include "app_state.h"
#include "pins.h"
#include "hcsr04.h"

#include "FreeRTOS.h"
#include "task.h"

/*
 * Dispara o HC-SR04 periodicamente, mede a distancia e deriva a velocidade
 * do bloco (Delta d / Delta t). Publica telemetria TLM_DIST.
 */
void sensor_distance_task(void *p) {
    (void) p;

    float prev_cm = 0.0f;
    TickType_t prev_tick = xTaskGetTickCount();
    bool have_prev = false;

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        hcsr04_trigger();

        if (hcsr04_wait_echo(HCSR04_TIMEOUT_US / 1000 + 1)) {
            float cm = hcsr04_last_distance_cm();
            TickType_t now = xTaskGetTickCount();

            float vel_cm_s = 0.0f;
            if (have_prev) {
                float dt = (now - prev_tick) * (1.0f / configTICK_RATE_HZ);
                if (dt > 0.0f) {
                    vel_cm_s = (cm - prev_cm) / dt;
                }
            }
            prev_cm = cm;
            prev_tick = now;
            have_prev = true;

            telemetry_push(TLM_DIST, now * (1000 / configTICK_RATE_HZ),
                           cm, vel_cm_s, 0.0f);
        }
        /* Sem eco: ignora esta amostra (objeto fora de alcance/erro). */

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(DIST_SAMPLE_MS));
    }
}
