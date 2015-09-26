#include "ds2480.h"
#include "ds1963s.h"

static int ds1963s_process(const unsigned char *bytes, size_t count, unsigned char *out, size_t *outsize, ibutton_t *button) {
    int i;
    *outsize = count;

    // TODO: unstub
    for(i = 0; i < count; ++i) {
        out[i] = bytes[i];
    }
    
    return count;
}

int ds1963s_init(ibutton_t *button, unsigned char *rom) {
    button->process = ds1963s_process;

    memcpy(button->rom, rom, 8);

    return 0;
}

