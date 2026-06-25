#include <libopencm3/stm32/rcc.h>

#include "app/actuators.h"
#include "app/app.h"
#include "app/sensors.h"
#include "app/tasks.h"
#include "drivers/uart_comm.h"
#include "platform/system_init.h"

/* Punto de entrada del firmware. */
int main(void)
{
    /* Configura la Blue Pill a 72 MHz usando cristal externo de 8 MHz. */
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

    /* Inicializacion comun de placa. */
    system_init_board();

    /* Inicializa LED PC13. */
    actuators_init();

    /* Inicializa sensores simulados. */
    sensors_init();

    /* Inicializa USART1 PA9/PA10 a 115200 8N1. */
    uart_comm_init();

    /* Reinicia contadores de aplicacion. */
    app_init();

    /* Crea tareas/colas y entrega el control a FreeRTOS. */
    tasks_start();

    /* No deberiamos volver aca; si ocurre, quedamos en loop seguro. */
    for (;;) {
    }
}
