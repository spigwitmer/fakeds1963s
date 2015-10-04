#include <stdio.h>
#include <stdlib.h>
#include "ds2480sim.h"
#include "ds1963s.h"
#include "extern/crcutil.h"

void print_hex(unsigned char *out, size_t len) {
    int i;
    for(i = 0; i < len; ++i) {
        printf("%x ", out[i]);
    }
    printf("\n");
}

int main() {
    unsigned char rom[8] = {0x18, '0', '1', '2', '7', '0', '7', 0x55};
    unsigned short crc, crc2;
    unsigned char what[] = {0xa5};
    unsigned char timing_buf[1] = {0xC1}, 
        detect_buf[5] = {
            CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p37Vus, 
            CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write10us, 
            CMD_CONFIG | PARMSEL_SAMPLEOFFSET | PARMSET_SampOff8us, 
            CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3), 
            CMD_COMM | FUNCTSEL_BIT | PARMSET_9600 | BITPOL_ONE
        },
        searchaccel_buf[23] = {
            MODE_DATA,
            0xF0,
            MODE_COMMAND,
            CMD_COMM | FUNCTSEL_SEARCHON | SPEEDSEL_FLEX,
            MODE_DATA,
            0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 
            MODE_COMMAND,
            CMD_COMM | FUNCTSEL_SEARCHOFF | SPEEDSEL_FLEX,
        },
        overdrive_buf[] = {0xc9},
        searchrom_buf[] = {
0xe1, 0xf0, 0xdb, 0xfe, 0x6d, 0xdb, 0xf6, 0x6f, 0xdf, 0xf6, 0x6f, 0xfb, 0xf6, 0x6f, 0xff, 0xf7, 0x6f, 0xdb, 0xf6, 0x6f, 0xff, 0xf7, 0x6f, 0xdf, 0xf7, 0x7d
        },
        idontknow_buf[] = {
0xa5, 0x20, 0x1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        outbuf[512];
    size_t outlen;


    ibutton_t button;
    ds1963s_init(&button, rom);

    ds2480_state_t ds2480;
    ds2480_init(&ds2480, &button);

    ///////
    outlen = 512;
    print_hex(timing_buf, 1);
    ds2480_process(timing_buf, 1, outbuf, &outlen, &ds2480);
    printf("outlen 1: %d\n", outlen);
    print_hex(outbuf, outlen);

    outlen = 512;
    print_hex(detect_buf, 5);
    ds2480_process(detect_buf, 5, outbuf, &outlen, &ds2480);
    printf("outlen 2: %d\n", outlen);
    print_hex(outbuf, outlen);

    outlen = 512;
    print_hex(searchaccel_buf, sizeof(searchaccel_buf));
    ds2480_process(searchaccel_buf, sizeof(searchaccel_buf), outbuf, &outlen, &ds2480);
    printf("outlen 2: %d\n", outlen);
    print_hex(outbuf, outlen);

    outlen = 512;
    print_hex(overdrive_buf, sizeof(overdrive_buf));
    ds2480_process(overdrive_buf, sizeof(overdrive_buf), outbuf, &outlen, &ds2480);
    printf("outlen 2: %d\n", outlen);
    print_hex(outbuf, outlen);

    outlen = 512;
    print_hex(searchrom_buf, sizeof(searchrom_buf));
    ds2480_process(searchrom_buf, sizeof(searchrom_buf), outbuf, &outlen, &ds2480);
    printf("outlen 2: %d\n", outlen);
    print_hex(outbuf, outlen);

    outlen = 512;
    print_hex(idontknow_buf, sizeof(idontknow_buf));
    ds2480_process(idontknow_buf, sizeof(idontknow_buf), outbuf, &outlen, &ds2480);
    printf("outlen 2: %d\n", outlen);
    print_hex(outbuf, outlen);

    printf("crc buffer size = %d\n", outlen-10);
    crc = full_crc16(outbuf, outlen-10, 0);
    printf("crc = %hx\n", crc);

    crc2 = full_crc16(what, 1, 0);
    printf("crc2 = %hx\n", crc2);

    return 0;
}
