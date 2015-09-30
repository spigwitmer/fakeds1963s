#include <stdio.h>
#define DEBUG
#include "ownet.h"
#include "shaib.h"

#define DUMP_OW_ERRS() { while(owHasErrors()) owPrintErrorMsgStd(); }

static uchar ROM[8] = {0x18, 
    '0', '1', '2', '7', '0', '7',
    0x55};

int main(int argc, char **argv) {
    SHACopr copr;
    int i, wcc, j;
    unsigned char apbuf[32], firstScratchPad[32], readOut[32], subkey[47];

    for(j = 0; j < 5; ++j) {
        for (i = 0; i < 47; ++i) {
            subkey[i] = 30 + i;
        }
        memset(readOut, 0x0, 32);
        memset(firstScratchPad, 0x0, 32);
        memcpy(apbuf, subkey, 32);

        memcpy(&firstScratchPad[8], &subkey[32], 15);

        copr.portnum = owAcquireEx("/dev/tty1963s");
        if (copr.portnum == -1) {
            printf("portnum is -1, bad...\n");
            while (owHasErrors()) {
                owPrintErrorMsgStd();
            }
        } else {
            printf("acquired\n");
            if (owHasErrors()) {
                printf("...and yet still has errors..\n");
                DUMP_OW_ERRS()
                return 0;
            }
            if (FindNewSHA(copr.portnum, copr.devAN, TRUE) == 0) {
                printf("FindNewSHA failed on its own..\n");
                DUMP_OW_ERRS()
                return 0;
            }
            if (WriteDataPageSHA18(copr.portnum, 0, apbuf, 1) == 0) {
                printf("WriteDataPageSHA18 failed on its own..\n");
                DUMP_OW_ERRS()
                return 0;
            }
            if (WriteScratchpadSHA18(copr.portnum, 0, firstScratchPad, 32, 1) == 0) {
                printf("WriteScratchpadSHA18 failed on its own..\n");
                DUMP_OW_ERRS()
                return 0;
            }
            if (SHAFunction18(copr.portnum, 0xC3, 0, 1) == 0) {
                printf("SHAFunction18 failed on its own..\n");
                DUMP_OW_ERRS()
                return 0;
            }
            if (ReadScratchpadSHA18(copr.portnum, 0, 0, readOut, 1) == 0) {
                printf("ReadScratchpadSHA18 failed on its own..\n");
                DUMP_OW_ERRS()
                return 0;
            }
            printf("SCRATCHPAD READOUT...\n");
            for (i = 0; i < 32; ++i) {
                printf("%x ", readOut[i]);
                if (i % 8 == 7)
                    printf("\n");
            }
            printf("\n");
            owRelease(copr.portnum);
        }
    }
    return 0;
}
