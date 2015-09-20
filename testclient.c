#include <stdio.h>
#include <stdlib.h>
#include "ds2480.h"
#include "ds1963s.h"

void print_hex(unsigned char *out, size_t len) {
    int i;
    for(i = 0; i < len; ++i) {
        printf("%x ", out[i]);
    }
    printf("\n");
}

int main() {
    unsigned char timing_buf[1] = {0xC1}, 
        detect_buf[5] = {
            CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p37Vus, 
            CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write10us, 
            CMD_CONFIG | PARMSEL_SAMPLEOFFSET | PARMSET_SampOff8us, 
            CMD_CONFIG | PARMSEL_PARMREAD | (PARMSEL_BAUDRATE >> 3), 
            CMD_COMM | FUNCTSEL_BIT | PARMSET_9600 | BITPOL_ONE
        },
        outbuf[512];
    size_t outlen;


    ibutton_t button;
    ds1963s_init(&button);

    ds2480_state_t ds2480;
    ds2480_init(&ds2480, &button);

    ///////
    outlen = 512;
    ds2480_process(timing_buf, 1, outbuf, &outlen, &ds2480);
    print_hex(timing_buf, 1);
    printf("outlen 1: %d\n", outlen);
    print_hex(outbuf, outlen);

    outlen = 512;
    ds2480_process(detect_buf, 5, outbuf, &outlen, &ds2480);
    print_hex(detect_buf, 5);
    printf("outlen 2: %d\n", outlen);
    print_hex(outbuf, outlen);

    return 0;
}
