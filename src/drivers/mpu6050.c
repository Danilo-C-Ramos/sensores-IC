#include "mpu6050.h"
#include "pins.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

/* Registradores do MPU6050 */
#define MPU_ADDR            0x68
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG          0x1A
#define REG_GYRO_CONFIG     0x1B
#define REG_ACCEL_CONFIG    0x1C
#define REG_ACCEL_XOUT_H    0x3B
#define REG_PWR_MGMT_1      0x6B
#define REG_WHOAMI          0x75

/* Fatores de conversao para os fundos de escala escolhidos:
 * accel +-2g  -> 16384 LSB/g
 * gyro  +-250 -> 131.0 LSB/(deg/s) */
#define ACCEL_LSB_PER_G     16384.0f
#define GYRO_LSB_PER_DPS    131.0f

static bool write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    int r = i2c_write_blocking(MPU_I2C_PORT, MPU_ADDR, buf, 2, false);
    return r == 2;
}

bool mpu6050_init(void) {
    i2c_init(MPU_I2C_PORT, MPU_I2C_BAUD);
    gpio_set_function(MPU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MPU_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(MPU_SDA_PIN);
    gpio_pull_up(MPU_SCL_PIN);

    /* Confere identidade */
    uint8_t who = 0;
    uint8_t reg = REG_WHOAMI;
    if (i2c_write_blocking(MPU_I2C_PORT, MPU_ADDR, &reg, 1, true) != 1) return false;
    if (i2c_read_blocking(MPU_I2C_PORT, MPU_ADDR, &who, 1, false) != 1) return false;
    if (who != 0x68) return false;

    /* Acorda (sai do sleep), clock do giro X */
    if (!write_reg(REG_PWR_MGMT_1, 0x01)) return false;
    /* DLPF ~44 Hz para reduzir ruido */
    if (!write_reg(REG_CONFIG, 0x03)) return false;
    /* Sample rate = 1kHz/(1+9) = 100 Hz */
    if (!write_reg(REG_SMPLRT_DIV, 0x09)) return false;
    /* Gyro +-250 deg/s */
    if (!write_reg(REG_GYRO_CONFIG, 0x00)) return false;
    /* Accel +-2g */
    if (!write_reg(REG_ACCEL_CONFIG, 0x00)) return false;

    return true;
}

bool mpu6050_read(mpu6050_data_t *out) {
    uint8_t reg = REG_ACCEL_XOUT_H;
    uint8_t raw[14];

    if (i2c_write_blocking(MPU_I2C_PORT, MPU_ADDR, &reg, 1, true) != 1) return false;
    if (i2c_read_blocking(MPU_I2C_PORT, MPU_ADDR, raw, 14, false) != 14) return false;

    int16_t ax = (int16_t)((raw[0]  << 8) | raw[1]);
    int16_t ay = (int16_t)((raw[2]  << 8) | raw[3]);
    int16_t az = (int16_t)((raw[4]  << 8) | raw[5]);
    int16_t t  = (int16_t)((raw[6]  << 8) | raw[7]);
    int16_t gx = (int16_t)((raw[8]  << 8) | raw[9]);
    int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
    int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

    out->ax = ax / ACCEL_LSB_PER_G;
    out->ay = ay / ACCEL_LSB_PER_G;
    out->az = az / ACCEL_LSB_PER_G;
    out->gx = gx / GYRO_LSB_PER_DPS;
    out->gy = gy / GYRO_LSB_PER_DPS;
    out->gz = gz / GYRO_LSB_PER_DPS;
    out->temp_c = t / 340.0f + 36.53f;

    return true;
}
