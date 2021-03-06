#include "ds2480sim.h"
#include "ds1963s.h"
#include "extern/crcutil.h"
#include "extern/sha1.h"

typedef enum {
    CMD_ROM,
    CMD_MEMORY
} ds1963s_cmd_state;

struct _ds1963s_data {
    unsigned char nvram[512];
    unsigned char secrets[64];
    unsigned char scratchpad[32];
    unsigned int nvram_counter[8]; // W/C counters
    unsigned int secret_counter[8];
    unsigned char TA1, TA2, LATCH_ES; // registers
    unsigned char AUTH, CHLG, HIDE, RC, MATCH; // internal flags
    unsigned char SEC; // latch
    ds1963s_cmd_state cmd_state;
};
typedef struct _ds1963s_data ds1963s_data;

/*
 * Little-endian byte copy
 */
static inline void copy_int32_le(unsigned char *out, int what) {
    out[0] = what & 0xff;
    out[1] = (what >> 8) & 0xff;
    out[2] = (what >> 16) & 0xff;
    out[3] = (what >> 24) & 0xff;
}

/*
 * read NVRAM from TA1:TA2 then compute SHA in scratchpad
 */
static int _ds1963s_read_nvram(unsigned char *out, int len, ibutton_t *button, int write_cycle) {
    sha1nfo s;
    unsigned char M = 0;
    ds1963s_data *pdata = (ds1963s_data*)button->data;
    unsigned int addr = (int)pdata->TA1 + ((int)(pdata->TA2) << 8);

    DS_DBG_PRINT("Copying from NVRAM at 0x%X\n", addr);

    if (addr + len > 0x200) {
        DS_DBG_PRINT("addr + len = %08x, bailing...\n", addr+len);
        return -1;
    }

    if (addr > 0x1e0) {
        DS_DBG_PRINT("addr = %08x, bailing...\n", addr);
        memset(out, 0xff, len);
        return len;
    }

    DS_DBG_PRINT("NVRAM address %08x seems sane.\n", addr);

    sha1_init(&s);
    memcpy(out, &pdata->nvram[addr], len);
    if (addr/32 > 7 && addr/32 < 16) {
        unsigned char trailing_sha_data[] = {0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0xB8};
        int *pcycle = &(pdata->nvram_counter[(addr/32) - 8]), 
            *scycle = &(pdata->secret_counter[(addr/32) - 8]);

        // 4-byte secret
        sha1_write(&s, &pdata->secrets[(addr/32 * 4) - 8], 4);

        // data page
        sha1_write(&s, &pdata->nvram[addr], len);

        // W/C counter
        sha1_write(&s, (unsigned char *)&pdata->nvram_counter[(addr/32) - 8], 4);

        // page number + M
        M = (uint8_t)(addr >> 5);
        if (pdata->MATCH && pdata->SEC && ((pdata->SEC & 6) == (pdata->TA1 & 0xE0)>>5)) {
            M |= 0x80;
        }
        sha1_writebyte(&s, M);

        // family code, serial, and ROM crc8
        sha1_write(&s, button->rom, 7);

        // 3-byte scratchpad challenge
        sha1_write(&s, &pdata->scratchpad[20], 3);

        // trailing static data
        sha1_write(&s, trailing_sha_data, 9);

        memcpy(&pdata->scratchpad[8], sha1_result(&s), 20);
        DS_DBG_PRINT("NVRAM write cycle count: %d\n", *pcycle);
        DS_DBG_PRINT("secret write cycle count: %d\n", *scycle);
        copy_int32_le(out+32, *pcycle);
        copy_int32_le(out+36, *scycle);
    }
    return len;
}

static int _ds1963s_sha_sign_data_page(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

    int addr, page;
    unsigned char mpx, trailing_sha_data[] = {0x80, 0x0, 0x0, 0x0, 
                                0x0, 0x0, 0x0, 0x1, 0xB8};
    ds1963s_data *pdata = (ds1963s_data*)button->data;
    sha1nfo s;

    sha1_init(&s);
    addr = pdata->TA1 + (pdata->TA2 << 8);
    page = addr / 32;
    if ((page == 0 || page == 8) && addr <= 0x1e0) {
        DS_DBG_PRINT("Signing with secret %d\n", page);
        sha1_write(&s, &pdata->secrets[page*4], 4);
        sha1_write(&s, &pdata->nvram[addr], 32);
        sha1_write(&s, &pdata->scratchpad[8], 4);
        mpx = pdata->scratchpad[12] & 0x1F;
        // M Control bit
        if (pdata->MATCH && pdata->SEC && ((pdata->SEC & 6) == (pdata->TA1 & 0xE0)>>5)) {
            mpx |= 0x80;
        }
        sha1_writebyte(&s, mpx);
        sha1_write(&s, &pdata->scratchpad[13], 7);
        sha1_write(&s, &pdata->secrets[(page*4)+1], 4);
        sha1_write(&s, &pdata->scratchpad[20], 3);
        sha1_write(&s, trailing_sha_data, 9);
        memcpy(&pdata->scratchpad[8], sha1_result(&s), 20);
    }
    pdata->CHLG = pdata->AUTH = 0;
    return 0;
}

static int ds1963s_process_sha(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {
    int processed;
    switch(bytes[0]) {
        case 0xC3: // sign data page
            processed = _ds1963s_sha_sign_data_page(bytes, count, out, outsize, overdrive, button);
            break;
        default:
            processed = count;
            break;
    }
    return processed;
}

static int _ds1963s_read_scratchpad(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

    int addr;
    ds1963s_data *pdata = (ds1963s_data*)button->data;
    unsigned short data_crc16;

    DS_DBG_PRINT("DS1963S: read scratchpad\n");
    out[1] = pdata->TA1;
    out[2] = pdata->TA2;
    if (pdata->HIDE == 1) {
        memset(&out[4], 0xFF, count-4);
    } else {
        addr = pdata->TA1 & 0x1F;
        DS_DBG_PRINT("Reading scratchpad from address %hx\n", addr);
        memcpy(&out[4], &pdata->scratchpad[addr], 32-addr);
        pdata->LATCH_ES |= 0x1F;
        out[3] = pdata->LATCH_ES;
        data_crc16 = full_crc16(out, 36, 0) ^ 0xffff;
        out[36] = data_crc16 & 0xff;
        out[37] = (data_crc16 >> 8) & 0xff;
        memset(&out[38], 0x55, count-38);
    }
    return count;
}

static int _ds1963s_write_scratchpad(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

    int addr, len;
    unsigned short data_crc16;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    DS_DBG_PRINT("DS1963S: write scratchpad\n");
    pdata->TA1 = bytes[1];
    pdata->TA2 = bytes[2];
    addr = pdata->TA1 + (pdata->TA2 << 8);
    if (addr < 0x200) {
        addr = pdata->TA1 & 0x1F;
        pdata->LATCH_ES &= 0x1F;
        len = 32 - addr;
        DS_DBG_PRINT("Writing scratchpad at address %hx\n", addr);
        memcpy(&pdata->scratchpad[addr], &bytes[3], len);
        memcpy(&out[3], &pdata->scratchpad[addr], len);
        data_crc16 = full_crc16(out, 35, 0) ^ 0xffff;
        out[35] = data_crc16 & 0xff;
        out[36] = (data_crc16 >> 8) & 0xff;
    }
    return count;
}

static int _ds1963s_erase_scratchpad(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

    ds1963s_data *pdata = (ds1963s_data*)button->data;

    DS_DBG_PRINT("DS1963S: erase scratchpad\n");
    pdata->TA1 = bytes[1];
    pdata->TA2 = bytes[2];
    memset(pdata->scratchpad, 0xFF, 32);
    pdata->HIDE = 0;
    memset(&out[3], 0x55, count-3);
    return count;
}

static int _ds1963s_copy_scratchpad(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

    int addr;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    pdata->CHLG = 0;
    pdata->AUTH = 0;
    addr = pdata->TA1 + (pdata->TA2 << 8);
    *outsize = count;
    // check HIDE flag and proper address
    if (    !(pdata->HIDE == 1 && addr >= 0x200 && addr < 0x23F)
         && !(pdata->HIDE == 0 && addr < 0x200) ) {
        memset(&out[4], 0xFF, count-4);
        return count;
    }
    // address registers should be exactly what was provided
    // by read scratchpad
    if (bytes[1] != pdata->TA1 || bytes[2] != pdata->TA2 || 
            bytes[3] != pdata->LATCH_ES) {
        DS_DBG_PRINT("DS1963S: copy scratchpad -- Address registers do not match!\n");
        memset(&out[4], 0xFF, count-4);
        return count;
    }
    pdata->LATCH_ES |= 0x80; // auth accepted
    if (pdata->HIDE == 0) {
        memcpy(&pdata->nvram[addr], pdata->scratchpad, pdata->LATCH_ES & 0x1F);
    } else {
        memcpy(&pdata->secrets[addr - 0x200], pdata->scratchpad, pdata->LATCH_ES & 0x1F);
    }
    // XXX: W/C counters
    memset(&out[4], 0x55, count-4);
    return count;
}

static int _ds1963s_read_auth_page(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {

    int page;
    unsigned short data_crc16;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    DS_DBG_PRINT("DS1963S: read auth page\n");
    pdata->TA1 = bytes[1];
    pdata->TA2 = bytes[2];
    page = (pdata->TA1 + (pdata->TA2 << 8)) / 32;
    out[1] = pdata->TA1;
    out[2] = pdata->TA2;
    _ds1963s_read_nvram(&out[3], 32, button, 1);
    data_crc16 = full_crc16(out, 43, 0) ^ 0xffff;
    out[43] = data_crc16 & 0xff;
    out[44] = (data_crc16 >> 8) & 0xff;
    memset(&out[45], 0x55, count-45);

    pdata->TA1 = pdata->TA2 = 0; 
    pdata->CHLG = pdata->AUTH = 0;
    return count;
}

static int ds1963s_process_memory(const unsigned char *bytes, size_t count, 
        unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button) {
    unsigned short data_crc16;
    ds1963s_data *pdata = (ds1963s_data*)button->data;

    memcpy(out, bytes, count);
    switch(out[0]) {
        case 0x33: // compute SHA
            DS_DBG_PRINT("DS1963S: compute sha (subfunction: %02x)\n", bytes[3]);
            pdata->TA1 = bytes[1];
            pdata->TA2 = bytes[2];
            data_crc16 = full_crc16(out, 4, 0) ^ 0xffff;
            out[4] = data_crc16 & 0xff;
            out[5] = (data_crc16 >> 8) & 0xff;
            *outsize = ds1963s_process_sha(&bytes[3], count-3, &out[3], outsize, overdrive, button) + count;
            memset(&out[6], 0x55, count-6);
            break;
        case 0xAA: // read scratchpad
            *outsize = _ds1963s_read_scratchpad(bytes, count, out, outsize, overdrive, button);
            break;
        case 0x0F: // write scratchpad
            *outsize = _ds1963s_write_scratchpad(bytes, count, out, outsize, overdrive, button);
            break;
        case 0xC3: // erase scratchpad
            *outsize = _ds1963s_erase_scratchpad(bytes, count, out, outsize, overdrive, button);
            break;
        case 0x55: // copy scratchpad
            *outsize = _ds1963s_copy_scratchpad(bytes, count, out, outsize, overdrive, button);
            break;
        case 0xA5: // read auth page
            *outsize = _ds1963s_read_auth_page(bytes, count, out, outsize, overdrive, button);
            break;
        default:
            DS_DBG_PRINT("DS1963S: unimplemented mem cmd: 0x%x\n", out[0]);
            memset(&out[1], 0xFF, count-1);
            break;
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

ibutton_t *ds1963s_init(unsigned char *rom) {
    ds1963s_data *pdata;
    ibutton_t *button = DS_MALLOC(sizeof(ibutton_t));
    if (!button)
        return NULL;

    button->process = ds1963s_process;
    button->reset_pulse = ds1963s_reset_pulse;
    memcpy(button->rom, rom, 8);
    button->data = DS_MALLOC(sizeof(ds1963s_data));
    if (!button->data) {
        DS_FREE(button);
        return NULL;
    }
    memset(button->data, 0x0, sizeof(ds1963s_data));

    pdata = button->data;

    pdata->cmd_state = CMD_ROM;
    pdata->HIDE = pdata->CHLG = pdata->AUTH = 0;
    pdata->TA1 = pdata->TA2 = pdata->LATCH_ES = 0;
    pdata->MATCH = 0;

    return button;
}

void ds1963s_destroy(ibutton_t *button) {
    DS_FREE(button->data);
    DS_FREE(button);
}
