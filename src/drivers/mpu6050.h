#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>

/*
 * Driver minimo do MPU6050 sobre I2C.
 * Fundo de escala configurado: acelerometro +-2g, giroscopio +-250 deg/s.
 */

typedef struct {
    float ax, ay, az;   /* aceleracao em g */
    float gx, gy, gz;   /* velocidade angular em deg/s */
    float temp_c;       /* temperatura em C */
} mpu6050_data_t;

/* Inicializa o I2C e acorda o sensor. Retorna false se o WHOAMI nao bater. */
bool mpu6050_init(void);

/* Le acelerometro, giroscopio e temperatura ja convertidos para unidades SI-ish.
 * Retorna false em erro de I2C. NAO faz lock -- proteja o barramento no chamador. */
bool mpu6050_read(mpu6050_data_t *out);

#endif /* MPU6050_H */
