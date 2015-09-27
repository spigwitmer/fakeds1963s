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
//--------------------------------------------------------------------------
//
//  crcutil.c - Keeps track of the CRC for 16 and 8 bit operations
//  version 2.00

// Local global variables

#define MAX_PORTNUM 10

static short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
static unsigned char dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};

//--------------------------------------------------------------------------
// Calculate a new CRC16 from the input data short.  Return the current
// CRC16 and also update the global variable CRC16.
//
// 'portnum'  - number 0 to MAX_PORTNUM-1.  This number is provided to
//              indicate the symbolic port number.
// 'data'     - data to perform a CRC16 on
//
// Returns: the current CRC16
//
static unsigned short crc16_iter(unsigned short seed, unsigned short cdata)
{
   cdata = (cdata ^ (seed & 0xff)) & 0xff;
   seed >>= 8;

   if (oddparity[cdata & 0xf] ^ oddparity[cdata >> 4])
     seed ^= 0xc001;

   cdata <<= 6;
   seed   ^= cdata;
   cdata <<= 1;
   seed   ^= cdata;

   return seed;
}

unsigned short full_crc16(unsigned char *data, int size, unsigned short seed) {
    int i;
    unsigned short curcrc = seed;
    for (i = 0; i < size; ++i) {
        curcrc = crc16_iter(curcrc, (unsigned short)data[i]);
    }
    return curcrc;
}

//--------------------------------------------------------------------------
// Update the Dallas Semiconductor One Wire CRC (utilcrc8) from the global
// variable utilcrc8 and the argument.
//
// 'portnum'  - number 0 to MAX_PORTNUM-1.  This number is provided to
//              indicate the symbolic port number.
// 'x'        - data byte to calculate the 8 bit crc from
//
// Returns: the updated utilcrc8.
//
static unsigned char crc8_iter(unsigned char seed, unsigned char x)
{
   return dscrc_table[seed ^ x];
}

unsigned char full_crc8(unsigned char *data, int size, unsigned short seed) {
    int i;
    unsigned char curcrc = seed;
    for (i = 0; i < size; ++i) {
        curcrc = crc8_iter(curcrc, (unsigned short)data[i]);
    }
    return curcrc;
}