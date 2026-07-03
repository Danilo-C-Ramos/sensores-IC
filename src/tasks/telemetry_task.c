#include "tasks.h"
#include "app_state.h"
#include "pins.h"

#include "hardware/uart.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stdio.h>

/*
 * Consome a fila de telemetria e emite linhas CSV pela UART do HC-06.
 * Formato (uma tag por fonte, facil de plotar/parsear no PC):
 *   D,t_ms,dist_cm,vel_cm_s
 *   I,t_ms,vel_imu_cm_s,tilt_deg,accel_g
 *   P,t_ms,tensao_V,corrente_A,potencia_W
 *   S,t_ms,modo
 *
 * Durante o bring-up (TELEMETRY_ALSO_USB=1, padrao) as mesmas linhas tambem
 * saem pela USB, permitindo testar cada modulo no monitor serial USB antes
 * de ter o HC-06 montado. Defina 0 na producao para nao gastar CPU na USB.
 */
#ifndef TELEMETRY_ALSO_USB
#define TELEMETRY_ALSO_USB 1
#endif

void telemetry_task(void *p) {
    (void) p;

    telemetry_sample_t s;
    char buf[80];

    for (;;) {
        if (xQueueReceive(xQueueTelemetry, &s, portMAX_DELAY) == pdTRUE) {
            int n = 0;
            switch (s.source) {
                case TLM_DIST:
                    n = snprintf(buf, sizeof(buf), "D,%lu,%.1f,%.1f\n",
                                 (unsigned long) s.t_ms, s.a, s.b);
                    break;
                case TLM_IMU:
                    n = snprintf(buf, sizeof(buf), "I,%lu,%.1f,%.1f,%.2f\n",
                                 (unsigned long) s.t_ms, s.a, s.b, s.c);
                    break;
                case TLM_POWER:
                    n = snprintf(buf, sizeof(buf), "P,%lu,%.2f,%.2f,%.2f\n",
                                 (unsigned long) s.t_ms, s.a, s.b, s.c);
                    break;
                case TLM_STATE:
                    n = snprintf(buf, sizeof(buf), "S,%lu,%d\n",
                                 (unsigned long) s.t_ms, (int) s.a);
                    break;
            }
            if (n > 0) {
                uart_write_blocking(HC06_UART_ID, (const uint8_t *) buf, (size_t) n);
#if TELEMETRY_ALSO_USB
                fputs(buf, stdout);   /* espelho na USB para debug de bancada */
#endif
            }
        }
    }
}
