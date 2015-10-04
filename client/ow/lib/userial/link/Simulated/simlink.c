//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//---------------------------------------------------------------------------
//
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>

#include "ds2480.h"
#include "ownet.h"
#include "ds2480sim.h"
#include "ds1963s.h"

ds2480_state_t *g_state;
unsigned char g_outbuffer[1024];
unsigned int g_bufsize;


//---------------------------------------------------------------------------
// Attempt to open a com port.  Keep the handle in ComID.
// Set the starting baud rate to 9600.
//
// 'portnum'   - number 0 to MAX_PORTNUM-1.  This number provided will
//               be used to indicate the port number desired when calling
//               all other functions in this library.
//
//
// Returns: the port number if it was succesful otherwise -1
//
int OpenCOMEx(char *port_zstr)
{
    int portnum = 0;
    if (g_state)
        return -1;

    if(!OpenCOM(portnum, port_zstr))
    {
        return -1;
    }
    return portnum;
}

//---------------------------------------------------------------------------
// Attempt to open a com port.
// Set the starting baud rate to 9600.
//
// 'portnum'   - number 0 to MAX_PORTNUM-1.  This number provided will
//               be used to indicate the port number desired when calling
//               all other functions in this library.
//
// 'port_zstr' - zero terminate port name.  For this platform
//               use format COMX where X is the port number.
//
//
// Returns: TRUE(1)  - success, COM port opened
//          FALSE(0) - failure, could not open specified port
//
SMALLINT OpenCOM(int portnum, char *port_zstr)
{
    unsigned char rom[8] = {0x18, // family code
    '0', '1', '2', '7', '0', '7',
    0x55}; // CRC8
    ibutton_t *button = ds1963s_init(rom);
    g_state = ds2480_init(button);

    return TRUE;
}


//---------------------------------------------------------------------------
// Closes the connection to the port.
//
// 'portnum'  - number 0 to MAX_PORTNUM-1.  This number was provided to
//              OpenCOM to indicate the port number.
//
void CloseCOM(int portnum)
{
    ds1963s_destroy(g_state->button);
    ds2480_destroy(g_state);
    g_state = NULL;
}


//--------------------------------------------------------------------------
// Write an array of bytes to the COM port, verify that it was
// sent out.  Assume that baud rate has been set.
//
// 'portnum'   - number 0 to MAX_PORTNUM-1.  This number provided will
//               be used to indicate the port number desired when calling
//               all other functions in this library.
// Returns 1 for success and 0 for failure
//
SMALLINT WriteCOM(int portnum, int outlen, uchar *outbuf)
{
    int i;

    size_t blen = 1024 - g_bufsize;
    i = ds2480_process(outbuf, outlen, g_outbuffer+g_bufsize, &blen, g_state);
    g_bufsize += blen;
    return (i != -1);
}


//--------------------------------------------------------------------------
// Read an array of bytes to the COM port, verify that it was
// sent out.  Assume that baud rate has been set.
//
// 'portnum'  - number 0 to MAX_PORTNUM-1.  This number was provided to
//              OpenCOM to indicate the port number.
// 'outlen'   - number of bytes to write to COM port
// 'outbuf'   - pointer ot an array of bytes to write
//
// Returns:  TRUE(1)  - success
//           FALSE(0) - failure
//
int ReadCOM(int portnum, int inlen, uchar *inbuf)
{
    int i;
    if (inlen > g_bufsize) {
        return 0;
    }
    memcpy(inbuf, g_outbuffer, inlen);
    memcpy(g_outbuffer, g_outbuffer+inlen, 1024-inlen);
    g_bufsize -= inlen;
    return inlen;
}


//---------------------------------------------------------------------------
//  Description:
//     flush the rx and tx buffers
//
// 'portnum'  - number 0 to MAX_PORTNUM-1.  This number was provided to
//              OpenCOM to indicate the port number.
//
void FlushCOM(int portnum)
{
    g_bufsize = 0;
}


//--------------------------------------------------------------------------
//  Description:
//     Send a break on the com port for at least 2 ms
//
// 'portnum'  - number 0 to MAX_PORTNUM-1.  This number was provided to
//              OpenCOM to indicate the port number.
//
void BreakCOM(int portnum)
{
}


//--------------------------------------------------------------------------
// Set the baud rate on the com port.
//
// 'portnum'   - number 0 to MAX_PORTNUM-1.  This number was provided to
//               OpenCOM to indicate the port number.
// 'new_baud'  - new baud rate defined as
// PARMSET_9600     0x00
// PARMSET_19200    0x02
// PARMSET_57600    0x04
// PARMSET_115200   0x06
//
void SetBaudCOM(int portnum, uchar new_baud)
{
}


//--------------------------------------------------------------------------
// Get the current millisecond tick count.  Does not have to represent
// an actual time, it just needs to be an incrementing timer.
//
long msGettick(void)
{
   struct timezone tmzone;
   struct timeval  tmval;
   long ms;

   gettimeofday(&tmval,&tmzone);
   ms = (tmval.tv_sec & 0xFFFF) * 1000 + tmval.tv_usec / 1000;
   return ms;
}


//--------------------------------------------------------------------------
//  Description:
//     Delay for at least 'len' ms
//
void msDelay(int len)
{
   struct timespec s;              // Set aside memory space on the stack

   s.tv_sec = len / 1000;
   s.tv_nsec = (len - (s.tv_sec * 1000)) * 1000000;
   nanosleep(&s, NULL);
}

