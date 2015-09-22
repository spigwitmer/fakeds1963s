#include <stdio.h>
#define DEBUG
#include "ownet.h"
#include "shaib.h"

#define DUMP_OW_ERRS() { while(owHasErrors()) owPrintErrorMsgStd(); }

int main(int argc, char **argv) {
    SHACopr copr;
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
/*
        if (FindNewSHA(copr.portnum, copr.devAN, TRUE) == 0) {
            printf("FindNewSHA failed on its own..\n");
            DUMP_OW_ERRS()
        }
*/
        owRelease(copr.portnum);
    }
    return 0;
}
