#include "power_sense.h"
#include "pins.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

#define ADC_AVG_SAMPLES 16

/* Zero do ACS712 (V na saida do sensor em 0 A), ajustavel em runtime. */
static float s_current_zero_v = ACS712_ZERO_V;

/* Le um canal do ADC com media (descarta a 1a leitura, como no codigo base). */
static float read_channel_volts(int channel) {
    adc_select_input(channel);
    adc_read();               /* descarta 1a leitura apos trocar de canal */
    sleep_us(5);

    uint32_t sum = 0;
    for (int i = 0; i < ADC_AVG_SAMPLES; i++) {
        sum += adc_read();
        sleep_us(50);
    }
    float counts = (float) sum / ADC_AVG_SAMPLES;
    return (counts / ADC_MAX_COUNTS) * ADC_VREF;
}

void power_sense_init(void) {
    adc_init();
    adc_gpio_init(ADC_VOLTAGE_PIN);
    adc_gpio_init(ADC_CURRENT_PIN);
}

float power_sense_read_voltage(void) {
    float v_adc = read_channel_volts(ADC_VOLTAGE_CHANNEL);
    return v_adc * VOLTAGE_DIVIDER_RATIO;
}

float power_sense_read_current(void) {
    float v_adc = read_channel_volts(ADC_CURRENT_CHANNEL);
    /* Desfaz o divisor da saida do ACS712 para recuperar a tensao real do sensor. */
    float v_acs = v_adc / ACS712_OUT_DIVIDER;
    return (v_acs - s_current_zero_v) / ACS712_SENS_V_PER_A;
}

void power_sense_calibrate_zero_current(void) {
    float v_adc = read_channel_volts(ADC_CURRENT_CHANNEL);
    s_current_zero_v = v_adc / ACS712_OUT_DIVIDER;
}
