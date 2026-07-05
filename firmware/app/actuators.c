#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "actuators.h"

#define STATUS_LED_PORT GPIOC
#define STATUS_LED_PIN  GPIO13


// Inicializa el pin de salida del LED de estado. PC13 en la Blue Pill es activo en bajo.
void actuators_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(STATUS_LED_PORT, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, STATUS_LED_PIN);
    gpio_set(STATUS_LED_PORT, STATUS_LED_PIN);
}


// Aplica un comando de actuador. Actualmente solo soporta el LED de estado.
void actuators_apply_command(const actuator_command_t *command)
{
    if (command == NULL) {
        return;
    }   //verifica la validez del puntero

    if (strcmp(command->target, "led") != 0) {
        return;
    }   //chequea que los strings sean iguales (verifica que el target sea "led")

    //Sabiendo que es un comando de led, actuamos:
    if (strcmp(command->action, "on") == 0) {
        gpio_clear(STATUS_LED_PORT, STATUS_LED_PIN);
        return;}
    if (strcmp(command->action, "off") == 0) {
        gpio_set(STATUS_LED_PORT, STATUS_LED_PIN);
        return;}
    if (strcmp(command->action, "toggle") == 0) {
        gpio_toggle(STATUS_LED_PORT, STATUS_LED_PIN);
    }
    else {
        return;
    }   //Si no es ninguno de los comandos válidos, no hace nada.
}
