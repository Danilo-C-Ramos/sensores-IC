#ifndef TASKS_H
#define TASKS_H

/*
 * Prototipos das tasks do sistema. Todas recebem void* (config/NULL) e
 * rodam em laco infinito. Criadas em main.c.
 *
 * Prioridades sugeridas (maior = mais urgente):
 *   control_task        4   (seguranca: freio)
 *   imu_task            3   (deteccao de balanco alimenta o freio)
 *   sensor_distance     2
 *   power_task          2
 *   command_task        2
 *   telemetry_task      1   (so I/O de saida)
 */

void control_task(void *p);
void imu_task(void *p);
void sensor_distance_task(void *p);
void power_task(void *p);
void command_task(void *p);
void telemetry_task(void *p);

/* Limiares de comportamento (ajustar na bancada) */
#define SWAY_TILT_ALARM_DEG   20.0f   /* balanco lateral que engata o freio */
#define IMU_SAMPLE_MS         10      /* ~100 Hz */
#define DIST_SAMPLE_MS        60      /* ~16 Hz (limite do HC-SR04) */
#define POWER_SAMPLE_MS       100     /* 10 Hz durante a descida */

#endif /* TASKS_H */
