#include "tasks.h"
#include "app_state.h"
#include "mpu6050.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>

/*
 * Le o MPU6050 a ~100 Hz:
 *  - filtro complementar -> angulo de inclinacao (balanco lateral);
 *  - integra a aceleracao linear -> 2a estimativa de velocidade do bloco
 *    (aproximada e sujeita a drift; corrigida por "leak" + reset em repouso);
 *  - dispara EVT_SWAY_ALARM quando o balanco passa do limiar.
 */

#define GRAVITY_MS2       9.81f
#define TILT_HYSTERESIS   3.0f     /* deg abaixo do limiar para rearmar o alarme */
#define VEL_LEAK          0.98f    /* fator anti-drift por amostra */
#define STATIONARY_G      0.05f    /* |a|-1g abaixo disso -> considera parado */

void imu_task(void *p) {
    (void) p;

    if (xSemaphoreTake(xMutexI2C, portMAX_DELAY) == pdTRUE) {
        mpu6050_init();
        xSemaphoreGive(xMutexI2C);
    }

    float tilt_deg = 0.0f;
    float vel_ms = 0.0f;
    bool sway_over = false;

    TickType_t wake = xTaskGetTickCount();
    const float dt = IMU_SAMPLE_MS / 1000.0f;

    for (;;) {
        mpu6050_data_t d;
        bool ok = false;

        if (xSemaphoreTake(xMutexI2C, portMAX_DELAY) == pdTRUE) {
            ok = mpu6050_read(&d);
            xSemaphoreGive(xMutexI2C);
        }

        if (ok) {
            /* Inclinacao a partir do acelerometro (angulo em relacao a vertical) */
            float horiz = sqrtf(d.ax * d.ax + d.ay * d.ay);
            float angle_accel = atan2f(horiz, d.az) * 57.2958f;   /* rad->deg */

            /* Taxa de giro horizontal (magnitude) para o filtro complementar */
            float gyro_rate = sqrtf(d.gx * d.gx + d.gy * d.gy);   /* deg/s */

            /* Filtro complementar: confia no giro no curto prazo, no accel no longo */
            tilt_deg = 0.98f * (tilt_deg + gyro_rate * dt) + 0.02f * angle_accel;

            /* 2a velocidade: integra a aceleracao linear (|a|-1g), com leak */
            float a_mag = sqrtf(d.ax * d.ax + d.ay * d.ay + d.az * d.az);
            float a_lin = (a_mag - 1.0f) * GRAVITY_MS2;           /* m/s^2 */
            if (fabsf(a_mag - 1.0f) < STATIONARY_G) {
                vel_ms = 0.0f;                                    /* repouso: reseta */
            } else {
                vel_ms = vel_ms * VEL_LEAK + a_lin * dt;
            }

            /* Alarme de balanco (com histerese) */
            if (!sway_over && tilt_deg > SWAY_TILT_ALARM_DEG) {
                sway_over = true;
                event_t e = { .type = EVT_SWAY_ALARM, .value = tilt_deg };
                xQueueSend(xQueueEvents, &e, 0);
            } else if (sway_over && tilt_deg < (SWAY_TILT_ALARM_DEG - TILT_HYSTERESIS)) {
                sway_over = false;
            }

            telemetry_push(TLM_IMU,
                           xTaskGetTickCount() * (1000 / configTICK_RATE_HZ),
                           vel_ms * 100.0f, tilt_deg, a_mag);
        } else {
            event_t e = { .type = EVT_SENSOR_FAULT, .value = 0.0f };
            xQueueSend(xQueueEvents, &e, 0);
        }

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(IMU_SAMPLE_MS));
    }
}
