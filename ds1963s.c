#include "ds2480.h"
#include "ds1963s.h"

static int ds1963s_process(const unsigned char *bytes, size_t count, unsigned char *out, size_t *outsize, ibutton_t *button) {
    int i, pos;
    *outsize = count;

    // TODO: unstub
    for(i = 0; i < count; ++i) {
        out[i] = bytes[i];
    }
    if (count > 1) {
        switch(out[0]) {
            case 0xF0: // search ROM
                DS_DBG_PRINT("DS1963S: search ROM\n");
                memset(&out[1], 0x0, 24);
                for (i = 0; i < 64; ++i) {
                    pos = i*3 + 1;
                    if (button->rom[i/8] & (1 << (i%8))) {
                        pos--;
                    }
                    out[(pos/8)+1] |= (unsigned char)(1 << (pos%8));
                }
                break;
        }
    }
    
    return count;
}

int ds1963s_init(ibutton_t *button, unsigned char *rom) {
    button->process = ds1963s_process;

    memcpy(button->rom, rom, 8);

    return 0;
}

