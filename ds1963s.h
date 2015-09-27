
/*
DS1963S simulator
*/

#pragma once

#include "ds2480sim.h"

int ds1963s_init(ibutton_t *button, unsigned char *rom);
void ds1963s_destroy(ibutton_t *button);
