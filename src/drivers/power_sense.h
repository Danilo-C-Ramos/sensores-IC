#ifndef POWER_SENSE_H
#define POWER_SENSE_H

/*
 * Medida de tensao e corrente do circuito na descida, via ADC da Pico.
 * Tensao: divisor resistivo. Corrente: ACS712/shunt.
 * As constantes de escala/calibracao ficam em pins.h.
 *
 * NAO faz lock do ADC -- proteja o barramento com xMutexADC no chamador.
 */

/* Configura o ADC e os dois canais. Chamar antes do scheduler. */
void power_sense_init(void);

/* Tensao do circuito em Volts (ja aplicando a razao do divisor). */
float power_sense_read_voltage(void);

/* Corrente do circuito em Amperes (ACS712, ja descontado o zero). */
float power_sense_read_current(void);

/* Recalibra o zero do sensor de corrente. Chamar com corrente = 0 A. */
void power_sense_calibrate_zero_current(void);

#endif /* POWER_SENSE_H */
