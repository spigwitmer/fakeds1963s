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
    int i;
    unsigned char apbuf[32];

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
        }
        if (FindNewSHA(copr.portnum, copr.devAN, TRUE) == 0) {
            printf("FindNewSHA failed on its own..\n");
            DUMP_OW_ERRS()
        }
        if (ReadAuthPageSHA18(copr.portnum, 9, apbuf, NULL, 0) == -1) {
            printf("ReadAuthPageSHA18 failed...\n");
            DUMP_OW_ERRS()
        }
        owRelease(copr.portnum);
    }
    return 0;
}
