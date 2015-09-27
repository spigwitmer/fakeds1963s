#include "ds2480sim.h"
#if defined(MODULE)
#include <linux/errno.h>
#else
#include <errno.h>
#endif

void ds2480_master_reset(ds2480_state_t *state) {
    state->mode = COMMAND;
    state->speed = SPEEDSEL_STD;
    state->search = 0;
    state->check = 0;
    state->search_rom_len = 0;

    memset(state->config, 0xaa, 256);

    state->config[PARMSEL_SLEW >> 4]                = PARMSET_Slew15Vus;
    state->config[PARMSEL_12VPULSE >> 4]            = PARMSET_512us;
    state->config[PARMSEL_5VPULSE >> 4]             = PARMSET_524ms;
    state->config[PARMSEL_WRITE1LOW >> 4]           = PARMSET_Write8us;
    state->config[PARMSEL_SAMPLEOFFSET >> 4]        = PARMSET_SampOff3us;
    state->config[PARMSEL_ACTIVEPULLUPTIME >> 4]    = 0; // unused?
    state->config[PARMSEL_BAUDRATE >> 4]            = PARMSET_9600;
}

void ds2480_soft_reset(ds2480_state_t *state) {
    state->mode = COMMAND;
    state->search = 0;
    state->check = 0;
    state->search_rom_len = 0;
}

int ds2480_init(ds2480_state_t *state, ibutton_t *button) {
    state->button = button;
    ds2480_master_reset(state);
    return 0;
}

#define OPUSH(b) { if (*outsize >= max_out-1) return -1; out[(*outsize)++] = (b); }

static int ds2480_process_cmd(const unsigned char *bytes, size_t count, 
            unsigned char *out, size_t *outsize, ds2480_state_t *state) {
    int i;
    size_t max_out = *outsize;
    *outsize = 0;

    for (i = 0; i < count; ++i) {
        DS_DBG_PRINT("CMD: 0x%x\n", bytes[i]);
        switch(bytes[i]) {
            case MODE_DATA:
                DS_DBG_PRINT("Switching to data mode....\n");
                state->mode = DATA;
                return i;
            case MODE_COMMAND:
                // we should never get here...
                DS_DBG_PRINT("MODE_COMMAND request within MODE_COMMAND, what?\n");
                continue;
        }
        if ((bytes[i] & CMD_COMM) == CMD_COMM) {
            DS_DBG_PRINT("%x is definitely a command...\n", bytes[i]);
            switch(bytes[i] & FUNCTSEL_MASK) {
                case FUNCTSEL_BIT:
                case 0x10:
                    DS_DBG_PRINT("FUNCTSEL_BIT\n");
                    state->speed = bytes[i] & SPEEDSEL_MASK;
                    OPUSH(bytes[i] & 0x9F)
                    break;
                    
                case FUNCTSEL_SEARCHOFF:
                    DS_DBG_PRINT("FUNCTSEL_SEARCHON/OFF\n");
                    state->search = (bytes[i] >> 4) & 1;
                    state->speed = bytes[i] & SPEEDSEL_MASK;
                    break;

                case FUNCTSEL_RESET:
                    DS_DBG_PRINT("FUNCTSEL_RESET\n");
                    if (state->button->reset_pulse)
                        state->button->reset_pulse(state->button);
                    state->speed = bytes[i] & SPEEDSEL_MASK;
                    OPUSH(0xCD) // always a pulse
                    break;

                case FUNCTSEL_CHMOD:
                case 0x70:
                    // TODO: pulse control for future EPROM devices?
                    // right now we don't care because the DS1963S
                    // doesn't have an EPROM to configure, but...
                    DS_DBG_PRINT("FUNCTSEL_CHMOD\n");
                    OPUSH((bytes[i] & 0x1C) | 0xE0)
                    break;
            }
        } else if ((bytes[i] & CMD_CONFIG) == CMD_CONFIG) {
            DS_DBG_PRINT("CMD_CONFIG: %d\n", (bytes[i] >> 4) & 0x07);
            if (bytes[i] & 0x70) {
                // write to config
                state->config[(bytes[i] >> 4) & 0x07] = (bytes[i] & 0x0E);
                OPUSH((bytes[i] & 0x70) | (state->config[(bytes[i] >> 4) & 0x07]))
            } else {
                // read from it
                OPUSH(state->config[(bytes[i] & 0x0E) >> 1])
            }
        }
    }
    return count;
}

static int ds2480_process_search_rom(unsigned char *out, ds2480_state_t *state) {
    int i;
    DS_DBG_PRINT("IT'S SEARCH ROM BUFFER TIME\n");

    memset(out, 0x0, 16);
    for (i = 0; i < 64; ++i) {
        if (state->button->rom[i/8] & (1 << (i%8))) {
            out[i/4] |= (unsigned char)(1 << (((i%4)*2)+1));
        }
    }

    return 0;
}

static int ds2480_process_data(const unsigned char *bytes, size_t count, 
            unsigned char *out, size_t *outsize, ds2480_state_t *state) {
    int i, switch_to_command = 0;
    size_t pcount = 0, num_cmd_bytes = 0;
    unsigned char *processed;


    /*
        Preprocess the data.
        A MODE_COMMAND while in data mode results in "check mode"...
        if a second MODE_COMMAND byte follows, it's a single data byte,
        otherwise it's a legitimate switch back to Command mode
    */
    processed = DS_MALLOC(count);
    if (!processed)
        return -ENOMEM;
    *outsize = 0;
    for (i = 0; i < count; ++i) {
        processed[pcount++] = bytes[i];
        if (bytes[i] == MODE_COMMAND) {
            if (i < count - 1 && bytes[i+1] == MODE_COMMAND) {
                ++num_cmd_bytes;
                ++i;
            } else {
                switch_to_command = 1;
                pcount--;
                break;
            }
        }
    }

    if (state->search == 1) {
        for (i = 0; i < pcount; ++i) {
            state->search_rom_buffer[state->search_rom_len++] = processed[i];
            if (state->search_rom_len >= 16) {
                ds2480_process_search_rom(out+(*outsize), state);
                state->search_rom_len = 0;
                *outsize += 16;
            }
        }
    } else {
        DS_DBG_PRINT("letting button process %lu bytes, speed = %x\n", pcount, state->speed);
        state->button->process(processed, pcount, out+(*outsize), 
                outsize, (state->speed == SPEEDSEL_OD), state->button);
    }

    if (switch_to_command == 1) {
        state->mode = COMMAND;
    }

    DS_FREE(processed);

    return pcount+num_cmd_bytes;
}

int ds2480_process(const unsigned char *bytes, size_t count, 
            unsigned char *out, size_t *outsize, ds2480_state_t *state) {

    unsigned int i;
    size_t max_out = *outsize, state_out;
    *outsize = 0;

    for (i = 0; i < count; ++i) {
        state_out = max_out-(*outsize);
        switch(state->mode) {
            case COMMAND:
                DS_DBG_PRINT("case CMD 1st byte: 0x%x\n", bytes[i]);
                i += ds2480_process_cmd(&bytes[i], count-i, out+(*outsize), &state_out, state);
                break;
            case DATA:
                DS_DBG_PRINT("case DATA 1st byte: 0x%x\n", bytes[i]);
                i += ds2480_process_data(&bytes[i], count-i, out+(*outsize), &state_out, state);
                break;
        }
        *outsize += state_out;
    }
    return count;
}

void ds2480_destroy(ds2480_state_t *state) {
    return;
}
