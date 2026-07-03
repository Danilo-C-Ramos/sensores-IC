#ifndef APP_STATE_H
#define APP_STATE_H

/*
 * Tipos e handles compartilhados entre as tasks.
 * Toda a comunicacao entre tasks passa por estas filas/mutex/event group,
 * criados em main.c antes do scheduler iniciar.
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

/* ---------------- Modo do sistema ---------------- */
typedef enum {
    MODE_IDLE = 0,   /* parado / peso seguro no topo */
    MODE_UP,         /* subindo (motor tracionando) */
    MODE_DOWN,       /* descendo (motor como gerador) */
    MODE_FAULT       /* falha/emergencia: freio engatado */
} system_mode_t;

/* Bits do event group publicados pela control_task.
 * Consumidos por tasks que so amostram em certos modos (ex.: power na descida). */
#define MODE_BIT_UP    (1 << 0)
#define MODE_BIT_DOWN  (1 << 1)

/* ---------------- Comandos (botoes + UART/BT) ---------------- */
typedef enum {
    CMD_UP = 0,      /* iniciar subida */
    CMD_DOWN,        /* iniciar descida */
    CMD_STOP,        /* parar / segurar */
    CMD_BRAKE_ON,    /* engatar freio manualmente */
    CMD_BRAKE_OFF,   /* soltar freio manualmente */
    CMD_CAL          /* recalibrar sensores (zero do IMU/ADC) */
} command_t;

/* ---------------- Eventos assincronos (sensor -> control) ---------------- */
typedef enum {
    EVT_SWAY_ALARM = 0,  /* MPU detectou balanco lateral excessivo */
    EVT_SENSOR_FAULT     /* falha de leitura de sensor */
} event_type_t;

typedef struct {
    event_type_t type;
    float value;         /* ex.: angulo de inclinacao que disparou o alarme */
} event_t;

/* ---------------- Amostra de telemetria ---------------- */
typedef enum {
    TLM_DIST = 0,   /* a=dist_cm,      b=vel_cm_s */
    TLM_IMU,        /* a=vel_imu_cm_s, b=tilt_deg,  c=accel_mag_g */
    TLM_POWER,      /* a=tensao_V,     b=corrente_A, c=potencia_W */
    TLM_STATE       /* a=modo (system_mode_t) */
} telemetry_source_t;

typedef struct {
    telemetry_source_t source;
    uint32_t t_ms;
    float a;
    float b;
    float c;
} telemetry_sample_t;

/* ---------------- Handles globais (definidos em main.c) ---------------- */
extern QueueHandle_t      xQueueCommands;    /* command_t   */
extern QueueHandle_t      xQueueEvents;      /* event_t     */
extern QueueHandle_t      xQueueTelemetry;   /* telemetry_sample_t */
extern SemaphoreHandle_t  xMutexI2C;         /* barramento MPU (e futuros devices) */
extern SemaphoreHandle_t  xMutexADC;         /* ADC compartilhado */
extern EventGroupHandle_t xEventGroupMode;   /* MODE_BIT_* */

/* Helper: enfileira uma amostra de telemetria sem bloquear (descarta se cheia). */
static inline void telemetry_push(telemetry_source_t src, uint32_t t_ms,
                                  float a, float b, float c) {
    telemetry_sample_t s = { .source = src, .t_ms = t_ms, .a = a, .b = b, .c = c };
    xQueueSend(xQueueTelemetry, &s, 0);
}

#endif /* APP_STATE_H */
