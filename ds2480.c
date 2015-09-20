#include "ds2480.h"

void ds2480_master_reset(ds2480_state_t *state) {
    state->mode = COMMAND;
    state->speed = SPEEDSEL_STD;

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
        if ((bytes[i] & CMD_COMM) == CMD_COMM) {
            switch(bytes[i] & 0x70) {
                case FUNCTSEL_BIT:
                case 0x10:
                    OPUSH(bytes[i] & 0x9F)
                    break;
                    
                case FUNCTSEL_SEARCHON:
                case FUNCTSEL_SEARCHOFF:
                    break;

                case FUNCTSEL_RESET:
                    OPUSH(0xCD) // always a pulse
                    break;

                case FUNCTSEL_CHMOD:
                    // TODO: pulse control for future EPROM devices?
                    // right now we don't care because the DS1963S
                    // doesn't have an EPROM to configure, but...
                    OPUSH((bytes[i] & 0x1C) | 0xE0)
                    break;
            }
        } else if ((bytes[i] & CMD_CONFIG) == CMD_CONFIG) {
            if (bytes[i] & 0x70) {
                // write to config
                state->config[(bytes[i] & 0x70) >> 4] = (bytes[i] & 0x0E);
                OPUSH(bytes[i] & 0x7E)
            } else {
                OPUSH(state->config[(bytes[i] & 0x70) >> 4])
            }
        }
    }
    return count;
}

static size_t ds2480_process_data(const unsigned char *bytes, size_t count, 
            unsigned char *out, size_t *outsize, ds2480_state_t *state) {
    int i;
    size_t max_out = *outsize;
    *outsize = 0;

    for (i = 0; i < count; ++i) {
        
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
                i += ds2480_process_cmd(&bytes[i], count-i, out+(*outsize), &state_out, state);
                break;
            case DATA:
                i += ds2480_process_data(&bytes[i], count-i, out+(*outsize), &state_out, state);
                break;
            case CHECK:
                break;
        }
        *outsize += state_out;
    }
    return count;
}
