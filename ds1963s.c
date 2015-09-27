#include "ds2480sim.h"
#include "ds1963s.h"

typedef enum {
    CMD_ROM,
    CMD_MEMORY,
    CMD_SHA
} ds1963s_cmd_state;

struct _ds1963s_data {
    unsigned char nvram[512];
    unsigned char scratchpad[32];
    unsigned char secrets[64];
    unsigned int nvram_counter[8];
    unsigned int secret_counter[8];
    unsigned char TA1, TA2, ES;
    unsigned char AUTH, CHLG, HIDE, RC;
    ds1963s_cmd_state cmd_state;
};
typedef struct _ds1963s_data ds1963s_data;


static int ds1963s_process_memory(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, ibutton_t *button) {
    int i;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    for(i = 0; i < count; ++i) {
        out[i] = bytes[i];
    }
    switch(out[0]) {
        case 0xA5: // read auth page
            DS_DBG_PRINT("DS1963S: read auth page\n");
            memset(&out[1], 0xFF, count-1);
            pdata->cmd_state = CMD_ROM;
            return count;
        default:
            DS_DBG_PRINT("DS1963S: unimplemented mem cmd: 0x%x\n", out[0]);
            pdata->cmd_state = CMD_ROM;
            return 1;
    }
}

static int ds1963s_process_rom(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, ibutton_t *button) {

    int processed = 0, i, pos;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    for(i = 0; i < count; ++i) {
        out[i] = bytes[i];
    }
    processed = count;
   
    switch(out[0]) {
        case 0x33: // read ROM
            DS_DBG_PRINT("DS1963S: read ROM\n");
            memcpy(out, button->rom, 8);
            processed = 9;
            break;
        case 0x55: // match ROM
            DS_DBG_PRINT("DS1963S: match ROM\n");
            memcpy(out, button->rom, 8);
            processed = 9;
            break;
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
            DS_DBG_PRINT("DS1963S: search ROM -- processed: %d\n", processed);
            break;
        case 0xCC: // skip ROM
        case 0xA5: // resume
            DS_DBG_PRINT("DS1963S: skip/resume\n");
            processed = 1;
            break;
        default:
            DS_DBG_PRINT("DS1963S: unimplemented cmd: 0x%x\n", out[0]);
            break;
    }
    *outsize = processed;
    pdata->cmd_state = CMD_MEMORY;
    return processed;
}

static int ds1963s_process_sha(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, ibutton_t *button) {
    // TODO
    return 0;
}

static int ds1963s_process(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {
    int i = 0;
    size_t state_out = 0;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    while (i < count) {
        state_out = 0;
        switch(pdata->cmd_state) {
            case CMD_ROM:
                DS_DBG_PRINT("DS1963S: processing in CMD_ROM mode\n");
                i += ds1963s_process_rom(&bytes[i], count, out+(*outsize), &state_out, button);
                break;
            case CMD_MEMORY:
                DS_DBG_PRINT("DS1963S: processing in CMD_MEMORY mode\n");
                i += ds1963s_process_memory(&bytes[i], count, out+(*outsize), &state_out, button);
                break;
            case CMD_SHA:
                DS_DBG_PRINT("DS1963S: processing in CMD_SHA mode\n");
                i += ds1963s_process_sha(&bytes[i], count, out+(*outsize), &state_out, button);
                break;
        }
        *outsize += state_out;
    }
    return count;
}

static int ds1963s_reset_pulse(ibutton_t *button) {
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    pdata->cmd_state = CMD_ROM;
}

int ds1963s_init(ibutton_t *button, unsigned char *rom) {
    ds1963s_data *pdata;

    button->process = ds1963s_process;
    button->reset_pulse = ds1963s_reset_pulse;
    memcpy(button->rom, rom, 8);
    button->data = DS_MALLOC(sizeof(ds1963s_data));
    if (!button->data)
        return -1;
    memset(button->data, 0x0, sizeof(ds1963s_data));
    pdata = button->data;

    pdata->cmd_state = CMD_ROM;

    return 0;
}

void ds1963s_destroy(ibutton_t *button) {
    DS_FREE(button->data);
}
