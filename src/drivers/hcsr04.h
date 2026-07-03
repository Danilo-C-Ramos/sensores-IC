#ifndef HCSR04_H
#define HCSR04_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Driver do sensor ultrassonico HC-SR04 com medida de eco por interrupcao.
 *
 * Uso tipico (dentro de uma task):
 *     hcsr04_trigger();
 *     if (hcsr04_wait_echo(30)) {         // espera ate 30 ms
 *         float cm = hcsr04_last_distance_cm();
 *     }
 *
 * ATENCAO: usa gpio_set_irq_enabled_with_callback(), que instala o UNICO
 * callback global de IRQ de GPIO do SDK. Nao use IRQ de GPIO para outros
 * pinos (ex.: botoes) em paralelo -- eles sao lidos por polling.
 */

/* Configura pinos, IRQ do eco e o semaforo interno. Chamar antes do scheduler. */
void hcsr04_init(void);

/* Dispara um pulso de 10 us no TRIG. */
void hcsr04_trigger(void);

/* Bloqueia a task ate o eco chegar (ou timeout). true = eco recebido. */
bool hcsr04_wait_echo(uint32_t timeout_ms);

/* Duracao do ultimo eco em microssegundos. */
uint32_t hcsr04_last_duration_us(void);

/* Distancia do ultimo eco em cm. */
float hcsr04_last_distance_cm(void);

#endif /* HCSR04_H */
