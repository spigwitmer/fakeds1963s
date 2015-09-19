#include "ds2480.h"

int ds2480_init(ds2480_state_t *state, ibutton_t *button) {
    state->mode = MODE_NORMAL;
    state->baud = PARMSET_9600;
    state->speed = SPEEDSEL_FLEX;

    state->button = button;

    return 0;
}

int ds2480_process(const unsigned char *bytes, size_t count, 
            unsigned char *out, size_t *outsize, ds2480_state_t *state) {

#define OPUSH(b) { if (*outsize >= max_out-1) return -1; out[(*outsize)++] = (b); }

    unsigned int i;
    size_t max_out = *outsize;
    *outsize = 0;
    for (i = 0; i < count; ++i) {
        if (bytes[i] == TIMING_BYTE) {
            continue; // client expects no response
        }
        if (bytes[i] & CMD_CONFIG) {
            unsigned char cfgbyte = bytes[i] & CMD_CONFIG;
            if (cfgbyte & PARMSEL_BAUDRATE) {
                OPUSH(PARMSET_9600)
            } else if (cfgbyte & PARMSEL_PARMREAD) {
                switch((cfgbyte & PARMSEL_PARMREAD) << 3) {
                    case PARMSEL_BAUDRATE:
                        OPUSH(state->baud);
                        break;
                    default:
                        OPUSH(0xff)
                }
            } else {
                OPUSH(0xff)
            }
            continue;
        } else if (bytes[i] & CMD_COMM) {
            unsigned char commbyte = bytes[i] & CMD_COMM;
            switch(commbyte & 0xF0) {
                case FUNCTSEL_BIT:
                    OPUSH(state->baud & 0x90)
                    break;
                case FUNCTSEL_SEARCHON:
                case FUNCTSEL_SEARCHOFF:
                case FUNCTSEL_RESET:
                case FUNCTSEL_CHMOD:
                    OPUSH(0xff)
                    break;
            }
            continue;
        }
    }

    return 0;

#undef OPUSH
}
