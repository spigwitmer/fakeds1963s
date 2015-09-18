#include <stdio.h>
#define DEBUG
#include "ownet.h"
#include "shaib.h"

int main(int argc, char **argv) {
    int portnum = owAcquireEx("/dev/tty1963s");
    if (portnum == -1) {
        printf("portnum is -1, bad...\n");
        while (owHasErrors()) {
            owPrintErrorMsgStd();
        }
    } else {
        printf("all systems go\n");
        owRelease(portnum);
    }
    return 0;
}
