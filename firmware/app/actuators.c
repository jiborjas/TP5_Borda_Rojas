#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "actuators.h"

/* Puerto donde esta conectado el LED integrado de la Blue Pill. */
#define STATUS_LED_PORT GPIOC

/* Pin del LED integrado; en Blue Pill suele ser PC13. */
#define STATUS_LED_PIN  GPIO13

/* Inicializa el hardware usado como actuador. */
void actuators_init(void)
{
    /* Habilitamos clock del puerto C porque PC13 vive ahi. */
    rcc_periph_clock_enable(RCC_GPIOC);

    /* Configuramos PC13 como salida push-pull lenta. */
    gpio_set_mode(STATUS_LED_PORT, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, STATUS_LED_PIN);

    /* PC13 es activo en bajo: ponerlo en alto deja el LED apagado. */
    gpio_set(STATUS_LED_PORT, STATUS_LED_PIN);
}

/* Aplica una orden recibida desde app.c. */
void actuators_apply_command(const actuator_command_t *command)
{
    /* Si no hay comando, no tocamos hardware. */
    if (command == NULL) {
        return;
    }

    /* Por ahora solo existe el actuador "led". */
    if (strcmp(command->target, "led") != 0) {
        return;
    }

    /* led=on: PC13 bajo, LED encendido. */
    if (strcmp(command->action, "on") == 0) {
        gpio_clear(STATUS_LED_PORT, STATUS_LED_PIN);
        return;
    }

    /* led=off: PC13 alto, LED apagado. */
    if (strcmp(command->action, "off") == 0) {
        gpio_set(STATUS_LED_PORT, STATUS_LED_PIN);
        return;
    }

    /* led=toggle: invertimos el estado actual. */
    if (strcmp(command->action, "toggle") == 0) {
        gpio_toggle(STATUS_LED_PORT, STATUS_LED_PIN);
    }
}
