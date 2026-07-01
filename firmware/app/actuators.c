#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "actuators.h"

#define STATUS_LED_PORT GPIOC
#define STATUS_LED_PIN  GPIO13

void actuators_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(STATUS_LED_PORT, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, STATUS_LED_PIN);
    gpio_set(STATUS_LED_PORT, STATUS_LED_PIN);
}

void actuators_apply_command(const actuator_command_t *command)
{
    if (command == NULL) {
        return;
    }

    if (strcmp(command->target, "led") != 0) {
        return;
    }

    if (strcmp(command->action, "on") == 0) {
        gpio_clear(STATUS_LED_PORT, STATUS_LED_PIN);
        return;
    }

    if (strcmp(command->action, "off") == 0) {
        gpio_set(STATUS_LED_PORT, STATUS_LED_PIN);
        return;
    }

    if (strcmp(command->action, "toggle") == 0) {
        gpio_toggle(STATUS_LED_PORT, STATUS_LED_PIN);
    }
}
