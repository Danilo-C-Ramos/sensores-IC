#include "actuator.h"
#include "pins.h"
#include "hardware/gpio.h"

static bool s_brake_engaged = false;

void actuator_init(void) {
    gpio_init(BRAKE_MOSFET_PIN);
    gpio_set_dir(BRAKE_MOSFET_PIN, GPIO_OUT);

    /* Estado seguro no boot: freio SOLTO. */
    s_brake_engaged = false;
    gpio_put(BRAKE_MOSFET_PIN, BRAKE_ACTIVE_LEVEL ? 0 : 1);

    /* LED de status do freio */
    gpio_init(LED_BRAKE_PIN);
    gpio_set_dir(LED_BRAKE_PIN, GPIO_OUT);
    gpio_put(LED_BRAKE_PIN, 0);

#ifdef MOTOR_RELAY_PIN
    gpio_init(MOTOR_RELAY_PIN);
    gpio_set_dir(MOTOR_RELAY_PIN, GPIO_OUT);
    gpio_put(MOTOR_RELAY_PIN, 0);
#endif
}

void actuator_set_brake(bool engaged) {
    s_brake_engaged = engaged;
    /* BRAKE_ACTIVE_LEVEL define qual nivel logico engata o freio. */
    int level = engaged ? BRAKE_ACTIVE_LEVEL : !BRAKE_ACTIVE_LEVEL;
    gpio_put(BRAKE_MOSFET_PIN, level);
    gpio_put(LED_BRAKE_PIN, engaged ? 1 : 0);
}

bool actuator_brake_engaged(void) {
    return s_brake_engaged;
}

void actuator_set_motor_relay(bool on) {
#ifdef MOTOR_RELAY_PIN
    gpio_put(MOTOR_RELAY_PIN, on ? 1 : 0);
#else
    (void) on;   /* fora do escopo atual: rele ainda nao instalado */
#endif
}
