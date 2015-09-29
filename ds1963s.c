#include "ds2480sim.h"
#include "ds1963s.h"
#include "crcutil.h"
#include "sha1.h"

#define SCRIPPIE_MODE 0

typedef enum {
    CMD_ROM,
    CMD_MEMORY
} ds1963s_cmd_state;

struct _ds1963s_data {
    unsigned char nvram[512];
    unsigned char secrets[64];
    unsigned char scratchpad[32];
    unsigned int nvram_counter[8];
    unsigned int secret_counter[8];
    unsigned char TA1, TA2, ES;
    unsigned char AUTH, CHLG, HIDE, RC;
    ds1963s_cmd_state cmd_state;
};
typedef struct _ds1963s_data ds1963s_data;

static inline void copy_int32_le(unsigned char *out, int what) {
    out[0] = what & 0xff;
    out[1] = (what >> 8) & 0xff;
    out[2] = (what >> 16) & 0xff;
    out[3] = (what >> 24) & 0xff;
}

// read NVRAM from TA1:TA2
static int _ds1963s_read_nvram(unsigned char *out, int len, ibutton_t *button, int write_cycle) {
    ds1963s_data *pdata = (ds1963s_data*)button->data;
    unsigned int addr = (int)pdata->TA1 + ((int)(pdata->TA2) << 8);

    DS_DBG_PRINT("Copying from NVRAM at 0x%X\n", addr);

#if !SCRIPPIE_MODE
    if (addr + len > 0x200) {
        return -1;
    }

    if (addr > 0x1e0) {
        memset(out, 0xff, len);
        return len;
    }
#endif

    memcpy(out, &pdata->nvram[addr], len);
    // increment W/C counter
    if (addr/32 > 7 && addr/32 < 16) {
        int *pcycle = &(pdata->nvram_counter[(addr/32) - 8]), 
            *scycle = &(pdata->secret_counter[(addr/32) - 8]);
        (*pcycle)++;
        DS_DBG_PRINT("NVRAM write cycle count: %d\n", *pcycle);
        DS_DBG_PRINT("secret write cycle count: %d\n", *scycle);
        copy_int32_le(out+32, *pcycle);
        copy_int32_le(out+36, *scycle);
    }
    return len;
}

static int ds1963s_process_sha(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {
    int i, addr, len, page;
    unsigned short data_crc16;
    ds1963s_data *pdata = (ds1963s_data*)button->data;
    sha1nfo s;

    sha1_init(&s);
    switch(bytes[0]) {
        case 0xC3:
            addr = pdata->TA1 + (pdata->TA2 << 8);
            page = addr / 32;
            if ((page == 0 || page == 8) && addr <= 0x1e0) {
                sha1_write(&s, &pdata->secrets[page*4], 8);
                sha1_write(&s, &pdata->nvram[addr], 32);
                sha1_write(&s, pdata->scratchpad, 15);
            } else {

            }
            break;
    }
    return 0;
}

static int ds1963s_process_memory(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {
    int i, addr, len, processed;
    unsigned short data_crc16;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    for(i = 0; i < count; ++i) {
        out[i] = bytes[i];
    }
    switch(out[0]) {
        case 0x33:
            pdata->TA1 = bytes[1];
            pdata->TA2 = bytes[2];
            *outsize = ds1963s_process_sha(bytes+3, count-3, out, outsize, overdrive, button) + 3;
            // CRC16
            break;
        case 0xAA: // read scratchpad
            DS_DBG_PRINT("DS1963S: read scratchpad\n");
            out[1] = pdata->TA1;
            out[2] = pdata->TA2;
            if (pdata->HIDE == 1) {
                memset(&out[4], 0xFF, count-4);
            } else {
                addr = pdata->TA1 & 0x1F;
                DS_DBG_PRINT("Reading scratchpad from address %hx\n", addr);
                memcpy(&out[4], &pdata->scratchpad[addr], 32-addr);
                pdata->ES |= 0x1F;
                out[3] = pdata->ES;
                data_crc16 = full_crc16(out, 36, 0) ^ 0xffff;
                out[36] = data_crc16 & 0xff;
                out[37] = (data_crc16 >> 8) & 0xff;
            }
            *outsize = count;
            break;
        case 0x0F: // write scratchpad
            DS_DBG_PRINT("DS1963S: write scratchpad\n");
            pdata->TA1 = bytes[1];
            pdata->TA2 = bytes[2];
            addr = pdata->TA1 + (pdata->TA2 << 8);
            if (addr < 0x200) {
                addr = pdata->TA1 & 0x1F;
                len = 32 - addr;
                DS_DBG_PRINT("Writing scratchpad at address %hx\n", addr);
                memcpy(&pdata->scratchpad[addr], &bytes[3], len);
                memcpy(&out[3], &pdata->scratchpad[addr], len);
                data_crc16 = full_crc16(out, 35, 0) ^ 0xffff;
                out[35] = data_crc16 & 0xff;
                out[36] = (data_crc16 >> 8) & 0xff;
            }
            *outsize = count;
            break;
        case 0xA5: // read auth page
            DS_DBG_PRINT("DS1963S: read auth page\n");
            pdata->TA1 = bytes[1];
            pdata->TA2 = bytes[2];
            out[1] = pdata->TA1;
            out[2] = pdata->TA2;
            _ds1963s_read_nvram(&out[3], 32, button, 1);
            data_crc16 = full_crc16(out, 43, 0) ^ 0xffff;
            out[43] = data_crc16 & 0xff;
            out[44] = (data_crc16 >> 8) & 0xff;
            out[count-1] = 0xA5;

            // TODO: SHA1 computation in scratchpad[8] for apps that actually
            // use this?
           
            pdata->TA1 = pdata->TA2 = 0; 
            *outsize = count;
            break;
        default:
            DS_DBG_PRINT("DS1963S: unimplemented mem cmd: 0x%x\n", out[0]);
            pdata->cmd_state = CMD_ROM;
            return 1;
    }
    pdata->cmd_state = CMD_ROM;
    return count;
}

static int ds1963s_process_rom(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

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
                i += ds1963s_process_rom(&bytes[i], count, out+(*outsize), &state_out, overdrive, button);
                break;
            case CMD_MEMORY:
                DS_DBG_PRINT("DS1963S: processing in CMD_MEMORY mode\n");
                i += ds1963s_process_memory(&bytes[i], count, out+(*outsize), &state_out, overdrive, button);
                break;
        }
        *outsize += state_out;
    }
    return count;
}

static int ds1963s_reset_pulse(ibutton_t *button) {
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    pdata->cmd_state = CMD_ROM;
    return 1;
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
    pdata->HIDE = pdata->TA1 = pdata->TA2 = pdata->ES = 0;

    return 0;
}

void ds1963s_destroy(ibutton_t *button) {
    DS_FREE(button->data);
}
