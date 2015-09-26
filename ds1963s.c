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
            default:
                DS_DBG_PRINT("DS1963S: cmd: 0x%x\n", out[0]);
                break;
        }
    }
    
    return count;
}

struct _ds1963s_data {
    unsigned char nvram[512];
    unsigned char scratchpad[32];
    unsigned char secrets[64];
    unsigned int nvram_counter[8];
    unsigned int secret_counter[8];
};
typedef struct _ds1963s_data ds1963s_data;

int ds1963s_init(ibutton_t *button, unsigned char *rom) {
    button->process = ds1963s_process;

    memcpy(button->rom, rom, 8);

    button->data = DS_MALLOC(sizeof(ds1963s_data));
    if (!button->data)
        return -1;
    memset(button->data, 0x0, sizeof(ds1963s_data));

    return 0;
}

void ds1963s_destroy(ibutton_t *button) {
    DS_FREE(button->data);
}
