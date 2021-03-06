//---------------------------------------------------------------------------
// Copyright (C) 2003 Dallas Semiconductor Corporation, All Rights Reserved.
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
//  pw77.c - Password functions for the DS1977.
//  version 1.00
//

// Include Files
#include "ownet.h"
#include "mbee77.h"
#include "pw77.h"
#include "mbscrx77.h"

#define READ_PSW_ADDRESS         0x7FC0
#define READWRITE_PSW_ADDRESS    0x7FC8
#define CONTROL_PSW_ADDRESS      0x7FD0
#define VERIFY_PSW_COMMAND       0xC3
#define PASSWORD_PAGE            511
#define PASSWORD_OFFSET          16
#define PAGE_LEN                 64


#define PASSWORD_INFO_PAGE       17
#define PASSWORD_INFO_OFFSET     7
#define PASSWORD_INFO_ADDR       0x220
// 1 byte, alternating ones and zeroes indicates passwords are enabled
/** Address of the Password Control Register. */
#define PASSWORD_CONTROL_REGISTER  0x227

// 8 bytes, write only, for setting the Read Access Password
/** Address of Read Access Password. */
#define READ_ACCESS_PASSWORD  0x228

// 8 bytes, write only, for setting the Read Access Password
/** Address of the Read Write Access Password. */
#define READ_WRITE_ACCESS_PASSWORD  0x230

uchar pswRO[8];
uchar pswRW[8];

SMALLINT passwordSetRO = FALSE;
SMALLINT passwordSetRW = FALSE;


/**
 * Set the READ or READ/WRITE password and verify it was written correctly.
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 * setpsw   Data to set 8 byte password 
 * type     Password type designator: (0) READ (1) READ/WRITE 
 *
 * @return 'true' if the read was complete
 */
SMALLINT setPassword(int portnum, uchar *SNum, uchar *setpsw, int type)
{
   int addr = ((type == READ_PSW) ? READ_PSW_ADDRESS : READWRITE_PSW_ADDRESS);
   SMALLINT i;
   uchar temp[32];
   uchar buf[64];

   if(SNum[0] == 0x41)
   {
      addr = PASSWORD_INFO_ADDR;

      if(readPageCRCEE77(1,portnum,SNum,PASSWORD_INFO_PAGE,temp))
      {
         if(temp[PASSWORD_INFO_OFFSET] == 0xAA)
         {
            OWERROR(OWERROR_PASSWORDS_ENABLED);
            return FALSE;
         }
      }
      else
      {
         OWERROR(OWERROR_CRC_FAILED);
         return FALSE;
      }

      for(i=0;i<8;i++)
         buf[i] = temp[i];

      if(type == READ_PSW)
      {
         for(i=0;i<8;i++)
            buf[i+8] = setpsw[i];
         for(i=0;i<8;i++)
            buf[i+16] = pswRW[i];
         for(i=24;i<32;i++)
            buf[i] = temp[i];
      }
      else
      {
         for(i=0;i<8;i++)
            buf[i+8] = pswRO[i];
         for(i=0;i<8;i++)
            buf[i+16] = setpsw[i];
         for(i=24;i<32;i++)
            buf[i] = temp[i];
      }

      if(writeEE77(1,portnum,SNum,addr,&buf[0],32))
      {
         for(i=0;i<32;i++)
            buf[i] = 0;
         writeScratchPadEx77(portnum,SNum,0,buf,32);
         if(type == READ_PSW)
         {
            for(i=0;i<8;i++)
               pswRO[i] = setpsw[i];
            passwordSetRO = TRUE;
         }
         else
         {
            for(i=0;i<8;i++)
               pswRW[i] = setpsw[i];
            passwordSetRW = TRUE;
         }
         return TRUE;
      }
   }
   else
   {
       // make sure that passwords are disabled
      if (readPageCRCEE77(1,portnum,SNum,PASSWORD_PAGE,buf))
      {
         // check byte
         if (buf[PASSWORD_OFFSET] == 0xAA)
         {
            OWERROR(OWERROR_PASSWORDS_ENABLED);
            return FALSE;
         }
      }
      else
      {
         OWERROR(OWERROR_CRC_FAILED);
         return FALSE;
      }

      // write the new password
      if (writeEE77(1,portnum,SNum,addr,setpsw,8))
      {
         // clear the scratchpad so no one can see it sitting there
         for (i = 0; i < PAGE_LEN; i++)
            buf[i] = 0;
         writeScratchPadEx77(portnum,SNum,0,buf,PAGE_LEN);

         // verify password was written correctly
         if (verifyPassword(portnum,SNum,setpsw,type))
            return TRUE;
         else
         {
            OWERROR(OWERROR_WRITE_VERIFY_FAILED);
            return FALSE;
         }
      }
   }

   OWERROR(OWERROR_WRITE_DATA_PAGE_FAILED);
   return FALSE;
}

/**
 * Set the READ or READ/WRITE password and verify it was written correctly.
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 * setpsw   Data to set 8 byte password 
 * type     Password type designator: (0) READ (1) READ/WRITE 
 *
 * @return 'true' if the read was complete
 */
SMALLINT verifyPassword(int portnum, uchar *SNum, uchar *setpsw, int type)
{
   SMALLINT i, send_len=0;
   uchar  raw_buf[15];
   int addr = ((type == READ_PSW) ? READ_PSW_ADDRESS : READWRITE_PSW_ADDRESS);

   // set serial number of device to read
   owSerialNum(portnum,SNum,FALSE);

   // select the device
   if (!owAccess(portnum))
   {
      OWERROR(OWERROR_DEVICE_SELECT_FAIL);
      return FALSE;
   }

   // command, address, offset, password (except last byte)
   raw_buf[send_len++] = VERIFY_PSW_COMMAND;
   raw_buf[send_len++] = addr & 0xFF;
   raw_buf[send_len++] = ((addr & 0xFFFF) >> 8) & 0xFF;

   for (i = 0; i < 7; i++)
      raw_buf[send_len++] = setpsw[i];
   
   // send block (check copy indication complete)
   if(!owBlock(portnum,FALSE,raw_buf,send_len))
   {
      OWERROR(OWERROR_BLOCK_FAILED);
      return FALSE;
   }

   // send last byte of password and enable strong pullup
   if (!owWriteBytePower(portnum, setpsw[7]))
   {
      OWERROR(OWERROR_WRITE_BYTE_FAILED);
      return FALSE;
   }

   // delay for read to complete
   msDelay(5);

   // turn off strong pullup
   owLevel(portnum, MODE_NORMAL);

   // read the confirmation byte
   if (owReadByte(portnum) != 0xAA)
   {
      OWERROR(OWERROR_READ_VERIFY_FAILED);
      return FALSE;
   }

   return TRUE;
}


/**
 * Enable or disabled passwords. 
 *
 * portnum  the port number of the port being used for the
 *          1-Wire Network.
 * SNum     the serial number for the part that the read is
 *          to be done on.
 * setpsw   Data to set 8 byte password 
 * mode     Mode designator: (0) DISABLED (1) ENABLED 
 *
 * @return 'true' if the read was complete
 */
SMALLINT setPasswordMode(int portnum, uchar *SNum, int mode)
{
   uchar buf[2];
   uchar temp[32];
   uchar data[32];
   int i;

   if(SNum[0] == 0x41)
   {
      if(mode)
      {
         if(passwordSetRO && passwordSetRW)
         {
            if(readPageCRCEE77(1,portnum,SNum,PASSWORD_INFO_PAGE,temp))
            {
               if(temp[PASSWORD_INFO_OFFSET] == 0xAA)
               {
                  OWERROR(OWERROR_PASSWORDS_ENABLED);
                  return FALSE;
               }
            }
            else
            {
               OWERROR(OWERROR_CRC_FAILED);
               return FALSE;
            }        
            
            for(i=0;i<7;i++)
               data[i] = temp[i];
            data[7] = 0xAA;

            for(i=0;i<8;i++)
               data[i+8] = pswRO[i];
            for(i=0;i<8;i++)
               data[i+16] = pswRW[i];
            for(i=24;i<32;i++)
               data[i] = temp[i];

            if(writeEE77(1,portnum,SNum,PASSWORD_INFO_ADDR,&data[0],32))
            {
               for(i=0;i<32;i++)
                  data[i] = 0;
               writeScratchPadEx77(portnum,SNum,0,data,32);
               return TRUE;
            }
         } 
         else
         {
            printf("RO value is %d RW value is %d\n",passwordSetRO,passwordSetRW);
            OWERROR(OWERROR_PASSWORD_NOT_SET);
            return FALSE;
         }
      }
      else
      {
         if(!readPageCRCEE77(1,portnum,SNum,PASSWORD_INFO_PAGE,temp))
         {
            OWERROR(OWERROR_CRC_FAILED);
            return FALSE;
         }        
            
         for(i=0;i<7;i++)
            data[i] = temp[i];
         data[7] = 0x00;

         for(i=0;i<8;i++)
            data[i+8] = pswRO[i];
         for(i=0;i<8;i++)
            data[i+16] = pswRW[i];
         for(i=24;i<32;i++)
            data[i] = temp[i];

         if(writeEE77(1,portnum,SNum,PASSWORD_INFO_ADDR,&data[0],32))
         {
            for(i=0;i<32;i++)
               data[i] = 0;
            writeScratchPadEx77(portnum,SNum,0,data,32);
            return TRUE;
         }
      }
   }
   else
   {
      // verify that the psw given is the correct READ/WRITE password
      if (verifyPassword(portnum,SNum,pswRW,READ_WRITE_PSW))
      {
         // write or clear
         buf[0] = (mode == ENABLE_PSW) ? 0xAA : 0;

         if (writeEE77(1,portnum,SNum,CONTROL_PSW_ADDRESS,buf,1))
            return TRUE;
         else
         {
            OWERROR(OWERROR_WRITE_BYTE_FAILED);
            return FALSE;
         }
      }  
      else
      {
         OWERROR(OWERROR_PASSWORD_INVALID);
         return FALSE;
      }
   }

   return FALSE;
}

void setBMPasswordRO(uchar *pw)
{
   int i;

   for(i=0;i<8;i++)
   {
      pswRO[i] = pw[i];
   }

}

/**
 * Set the read-only or read/write password for all the memory
 * bank functions. 
 *
 * pw       Data to set 8 byte password 
 */
void setBMPasswordRW(uchar *pw)
{
   int i;

   for(i=0;i<8;i++)
   {
      pswRW[i] = pw[i];
   }

}

/**
 * Get the read-only password for all the memory
 * bank functions. 
 *
 * pw       Data to set 8 byte password 
 */
void getBMPasswordRO(uchar *pw)
{
   int i;

   for(i=0;i<8;i++)
   {
      pw[i] = pswRO[i];
   }

}

/**
 * Get the read/write password for all the memory
 * bank functions. 
 *
 * pw       Data to set 8 byte password 
 */
void getBMPasswordRW(uchar *pw)
{
   int i;

   for(i=0;i<8;i++)
   {
      pw[i] = pswRW[i];
   }

}

/**
 * Return wether the R/W password was set
 */
SMALLINT readWritePasswordSet()
{
   return passwordSetRW;
}

/**
 * Return wether the RO password was set
 */
SMALLINT readOnlyPasswordSet()
{
   return passwordSetRO;
}

