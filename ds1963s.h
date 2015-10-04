
/*
DS1963S simulator
*/

#pragma once

#include "ds2480sim.h"

ibutton_t *ds1963s_init(unsigned char *rom);
void ds1963s_destroy(ibutton_t *button);
