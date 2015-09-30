#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>

struct termios origterm;

int open_tty1963s() {
    struct termios t;
    int fd, rc;
    fd = open("/dev/tty1963s", O_RDWR|O_NONBLOCK);
    rc = tcgetattr (fd, &t);
    cfsetospeed(&t, B9600);
    cfsetispeed (&t, B9600);

    // Get terminal parameters. (2.00) removed raw
    tcgetattr(fd,&t);
    // Save original settings.
    origterm = t;

    // Set to non-canonical mode, and no RTS/CTS handshaking
    t.c_iflag &= ~(BRKINT|ICRNL|IGNCR|INLCR|INPCK|ISTRIP|IXON|IXOFF|PARMRK);
    t.c_iflag |= IGNBRK|IGNPAR;
    t.c_oflag &= ~(OPOST);
    t.c_cflag &= ~(CRTSCTS|CSIZE|HUPCL|PARENB);
    t.c_cflag |= (CLOCAL|CS8|CREAD);
    t.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|IEXTEN|ISIG);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 3;

    rc = tcsetattr(fd, TCSAFLUSH, &t);
    if (rc < 0) {
        close(fd);
    }
    tcflush(fd,TCIOFLUSH);
    return fd;
}

int ReadCOM(int portnum, int inlen, uchar *inbuf)
{
   fd_set         filedescr;
   struct timeval tval;
   int            cnt;

   // loop to wait until each byte is available and read it
   for (cnt = 0; cnt < inlen; cnt++)
   {
      // set a descriptor to wait for a character available
      FD_ZERO(&filedescr);
      FD_SET(fd[portnum],&filedescr);
      // set timeout to 10ms
      tval.tv_sec = 0;
      tval.tv_usec = 10000;

      // if byte available read or return bytes read
      if (select(fd[portnum]+1,&filedescr,NULL,NULL,&tval) != 0)
      {
         if (read(fd,&inbuf[cnt],1) != 1)
            return cnt;
      }
      else
         return cnt;
   }

   // success, so return desired length
   return inlen;
}


int main() {
    unsigned char handshake[] = {0x17, 0x45, 0x5b, 0xf, 0x91};
    unsigned char buf[1024];
    int fd = open_tty1963s();
    if (fd == -1) {
        fprintf(stderr, "really...\n");
        return 1;
    }
    write(fd, handshake, 5);
    t
    return 0;
}
