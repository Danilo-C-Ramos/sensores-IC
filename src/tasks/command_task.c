#include "tasks.h"
#include "app_state.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>
#include <ctype.h>

/*
 * Unifica comando manual local (botoes, por polling) e remoto (linha de texto
 * pela UART do HC-06). Ambos viram command_t em xQueueCommands.
 *
 * Botoes sao lidos por polling (com debounce por borda) porque o unico callback
 * global de IRQ de GPIO do SDK ja pertence ao HC-SR04.
 */

#define BTN_POLL_MS   20

static void send_cmd(command_t c) {
    xQueueSend(xQueueCommands, &c, 0);
}

static void buttons_init(void) {
    const uint pins[] = { BTN_UP_PIN, BTN_DOWN_PIN, BTN_STOP_PIN };
    for (int i = 0; i < 3; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);   /* pressionado = 0 */
    }
}

/* Traduz uma linha de texto recebida por Bluetooth em comando. */
static void parse_line(char *line) {
    for (char *s = line; *s; s++) *s = (char) tolower((unsigned char) *s);

    if      (strncmp(line, "up", 2) == 0 || strncmp(line, "sub", 3) == 0)   send_cmd(CMD_UP);
    else if (strncmp(line, "down", 4) == 0 || strncmp(line, "desc", 4) == 0) send_cmd(CMD_DOWN);
    else if (strncmp(line, "stop", 4) == 0 || strncmp(line, "par", 3) == 0)  send_cmd(CMD_STOP);
    else if (strncmp(line, "brake", 5) == 0 || strncmp(line, "freio", 5) == 0) send_cmd(CMD_BRAKE_ON);
    else if (strncmp(line, "release", 7) == 0 || strncmp(line, "solt", 4) == 0) send_cmd(CMD_BRAKE_OFF);
    else if (strncmp(line, "cal", 3) == 0) send_cmd(CMD_CAL);
}

void command_task(void *p) {
    (void) p;

    buttons_init();

    /* Estado anterior dos botoes (true = solto, por causa do pull-up). */
    bool prev_up = true, prev_down = true, prev_stop = true;

    char line[48];
    int idx = 0;

    for (;;) {
        /* --- Botoes (deteccao de borda de descida = pressionar) --- */
        bool up   = gpio_get(BTN_UP_PIN);
        bool down = gpio_get(BTN_DOWN_PIN);
        bool stop = gpio_get(BTN_STOP_PIN);

        if (prev_up && !up)     send_cmd(CMD_UP);
        if (prev_down && !down) send_cmd(CMD_DOWN);
        if (prev_stop && !stop) send_cmd(CMD_STOP);
        prev_up = up; prev_down = down; prev_stop = stop;

        /* --- UART do HC-06 (monta linha ate \n) --- */
        while (uart_is_readable(HC06_UART_ID)) {
            char ch = uart_getc(HC06_UART_ID);
            if (ch == '\n' || ch == '\r') {
                if (idx > 0) {
                    line[idx] = '\0';
                    parse_line(line);
                    idx = 0;
                }
            } else if (idx < (int) sizeof(line) - 1) {
                line[idx++] = ch;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));
    }
}
