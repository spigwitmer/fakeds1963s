#include "ds2480.h"
#include "ds1963s.h"

static size_t ds1963s_process(const unsigned char byte, unsigned char *out, size_t *outsize, ibutton_t *button) {
    //size_t max_out = *outsize, state_out;
    *outsize = 0;

    DS_DBG_PRINT("Look at this shit they're making me process: 0x%x\n", byte);
    // TODO: unstub
    if (byte == 0xF0) {
        out[0] = 0xF0;
        *outsize = 1;
    }

    return 0;
}

int ds1963s_init(ibutton_t *button, unsigned char *rom) {
    button->process = ds1963s_process;

    memcpy(button->rom, rom, 8);

    return 0;
}

