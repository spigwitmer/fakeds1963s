#include "ds1963s.h"

static size_t ds1963s_process(const unsigned char *bytes, size_t count, unsigned char *out) {
    return 0; // TODO: unstub
}

int ds1963s_init(ibutton_t *button) {
    button->process = ds1963s_process;
}

