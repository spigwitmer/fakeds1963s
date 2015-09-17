#include <stdio.h>
#include "shaib.h"

extern int owGetErrorNum(void);
extern void owClearError(void);
extern int owHasErrors(void);
#ifdef DEBUG
   extern void owRaiseError(int,int,char*);
#else
   extern void owRaiseError(int);
#endif
#ifndef SMALL_MEMORY_TARGET
   extern void owPrintErrorMsg(FILE *); 
   extern void owPrintErrorMsgStd();
   extern char *owGetErrorMsg(int);
#endif

int main(int argc, char **argv) {
    int portnum = owAcquireEx("/dev/tty1963s");
    if (portnum == -1) {
        printf("portnum is -1, bad...\n");
        owPrintErrorMsgStd();
    } else {
        owRelease(portnum);
    }
    return 0;
}
