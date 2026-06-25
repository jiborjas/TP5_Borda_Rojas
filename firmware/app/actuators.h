#ifndef ACTUATORS_H
#define ACTUATORS_H

#include "app.h"

void actuators_init(void);
void actuators_apply_command(const actuator_command_t *command);

#endif
