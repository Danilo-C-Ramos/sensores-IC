#include "hcsr04.h"
#include "pins.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"

static volatile uint32_t s_start_us = 0;
static volatile uint32_t s_duration_us = 0;
static SemaphoreHandle_t s_echo_sem = NULL;

static void echo_callback(uint gpio, uint32_t events) {
    if (gpio != HCSR04_ECHO_PIN) return;

    if (events & GPIO_IRQ_EDGE_RISE) {
        s_start_us = time_us_32();
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
        s_duration_us = time_us_32() - s_start_us;
        BaseType_t hpw = pdFALSE;
        xSemaphoreGiveFromISR(s_echo_sem, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}

void hcsr04_init(void) {
    s_echo_sem = xSemaphoreCreateBinary();

    gpio_init(HCSR04_TRIG_PIN);
    gpio_set_dir(HCSR04_TRIG_PIN, GPIO_OUT);
    gpio_put(HCSR04_TRIG_PIN, 0);

    gpio_init(HCSR04_ECHO_PIN);
    gpio_set_dir(HCSR04_ECHO_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(HCSR04_ECHO_PIN,
                                       GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, &echo_callback);
}

void hcsr04_trigger(void) {
    /* Garante que um eventual semaforo pendente nao vaze para esta medida. */
    xSemaphoreTake(s_echo_sem, 0);

    gpio_put(HCSR04_TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(HCSR04_TRIG_PIN, 0);
}

bool hcsr04_wait_echo(uint32_t timeout_ms) {
    return xSemaphoreTake(s_echo_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

uint32_t hcsr04_last_duration_us(void) {
    return s_duration_us;
}

float hcsr04_last_distance_cm(void) {
    /* d = (t_us * 0.0343 cm/us) / 2  (ida e volta) */
    return (s_duration_us * 0.0343f) / 2.0f;
}
