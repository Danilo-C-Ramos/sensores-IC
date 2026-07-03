#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <stdbool.h>

/*
 * Camada de atuacao. Hoje controla apenas o freio eletromecanico
 * (MOSFET que da curto no motor). O rele de entrada da fonte de 24V
 * fica como expansao futura -- ver actuator_set_motor_relay() (stub).
 *
 * IMPORTANTE: para evitar corridas, apenas a control_task deve chamar
 * estas funcoes de escrita.
 */

/* Configura os GPIOs e deixa o freio no estado seguro (solto). Chamar antes
 * do scheduler iniciar. */
void actuator_init(void);

/* Engata (true) ou solta (false) o freio. */
void actuator_set_brake(bool engaged);

/* Estado atual do freio. */
bool actuator_brake_engaged(void);

/* Expansao futura: liga/desliga o rele da fonte de 24V do motor.
 * Stub por enquanto (fora do escopo atual). */
void actuator_set_motor_relay(bool on);

#endif /* ACTUATOR_H */
