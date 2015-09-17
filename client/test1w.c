#include <stdio.h>
#include "shaib.h"

int main(int argc, char **argv) {
    int portnum = owAcquireEx("/dev/tty1963s");
    if (portnum == -1) {
        printf("portnum is -1, bad...\n");
    } else {
        owRelease(portnum);
    }
    return 0;
}
