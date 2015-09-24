#include "ds2480.h"

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

int ds2480_init(ds2480_state_t *state, ibutton_t *button) {
    state->button = button;
    ds2480_master_reset(state);
    return 0;
}

#define OPUSH(b) { if (*outsize >= max_out-1) return -1; out[(*outsize)++] = (b); }

static size_t ds2480_process_cmd(const unsigned char *bytes, size_t count, 
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
            switch(bytes[i] & 0x60) {
                case FUNCTSEL_BIT:
                case 0x10:
                    DS_DBG_PRINT("FUNCTSEL_BIT\n");
                    OPUSH(bytes[i] & 0x9F)
                    break;
                    
                case FUNCTSEL_SEARCHOFF:
                    DS_DBG_PRINT("FUNCTSEL_SEARCHON/OFF\n");
                    state->search = (bytes[i] >> 4) & 1;
                    break;

                case FUNCTSEL_RESET:
                    DS_DBG_PRINT("FUNCTSEL_RESET\n");
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
                OPUSH(state->config[(bytes[i] & 0x0E) >> 1])
            }
        }
    }
    return count;
}

static int ds2480_process_search_rom(unsigned char *out, ds2480_state_t *state) {
    int i, j, pos;
    DS_DBG_PRINT("IT'S SEARCH ROM BUFFER TIME\n");

    for (i = 0; i < 8; ++i) {
        for (j = 7; j >= 0; --j) {
            if (state->button->rom[i] & (unsigned char)(0x80 >> j)) {
                pos = ((i*8) + j)*2;
                out[pos/8] |= (unsigned char)(0x80 >> (pos%8));
            }
        }
    }

    return 0;
}

static size_t ds2480_process_data(const unsigned char *bytes, size_t count, 
            unsigned char *out, size_t *outsize, ds2480_state_t *state) {
    int i;
    size_t max_out = *outsize, state_out;
    *outsize = 0;

    for (i = 0; i < count; ++i) {
        if (bytes[i] == MODE_COMMAND) {
            if (bytes[i+1] == MODE_COMMAND) {
                i += 1;
                state->button->process(MODE_COMMAND, out+(*outsize), &state_out, state->button);
                *outsize += state_out;
            } else {
                DS_DBG_PRINT("Switching from DATA to COMMAND mode\n");
                state->mode = COMMAND;
                return i;
            }
        } else if (state->search == 1) {
            state->search_rom_buffer[state->search_rom_len++] = bytes[i];
            if (state->search_rom_len >= 16) {
                ds2480_process_search_rom(out+(*outsize), state);
                state->search_rom_len = 0;
                *outsize += 16;
            }
        } else {
            state->button->process(bytes[i], out+(*outsize), &state_out, state->button);
            *outsize += state_out;
        }
    }
    return count;
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
