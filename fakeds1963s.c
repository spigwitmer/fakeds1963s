/**
 * fakeds1963s.c -- userspace emulation of a DS1963S iButton inside a
 *                  DS2480 serial housing.
 *
 * Opens a PTY pair and serves the slave path on stdout so that a client
 * (e.g. the 1-Wire Public Domain SDK's ownet layer) can talk to it
 * exactly the same way it would talk to the kernel tty1963s driver.
 *
 * Clients should respect the OWPORT environment variable; if unset we
 * fall back to /dev/tty1963s for compatibility with the kernel module.
 */

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "ds2480sim.h"
#include "ds1963s.h"

#define SLAVE_PATH_FALLBACK "/dev/tty1963s"

static unsigned char ROM[8] = {
    0x18,
    '0', '1', '2', '7', '0', '7',
    0x55
};

struct ctx {
    int mfd;
    int keepopen;
    ds2480_state_t *state;
};

static volatile sig_atomic_t g_stop;

static void on_signal(int signo) {
    (void)signo;
    g_stop = 1;
}

static void *reader_loop(void *arg) {
    struct ctx *c = (struct ctx *)arg;
    unsigned char inbuf[1024];
    unsigned char outbuf[1024];

    for (;;) {
        ssize_t n = read(c->mfd, inbuf, sizeof(inbuf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (g_stop)
                break;
            if (errno == EIO) {
                /* slave side was closed -- normally means client gone.
                 * keep our own slave fd open in the parent, so this
                 * should only happen on shutdown. */
                break;
            }
            perror("read(master)");
            break;
        }
        if (n == 0)
            continue;

        size_t outlen = sizeof(outbuf);
        ds2480_process(inbuf, (size_t)n, outbuf, &outlen, c->state);

        size_t written = 0;
        while (written < outlen) {
            ssize_t w = write(c->mfd, outbuf + written, outlen - written);
            if (w < 0) {
                if (errno == EINTR)
                    continue;
                perror("write(master)");
                return NULL;
            }
            written += (size_t)w;
        }
    }
    return NULL;
}

static int open_pty(int *mfd_out, int *keepopen_out, char *slave_out, size_t slave_len) {
    int mfd = posix_openpt(O_RDWR | O_NOCTTY);
    if (mfd < 0) {
        perror("posix_openpt");
        return -1;
    }
    if (grantpt(mfd) < 0 || unlockpt(mfd) < 0) {
        perror("grantpt/unlockpt");
        close(mfd);
        return -1;
    }

    char *name = ptsname(mfd);
    if (!name) {
        perror("ptsname");
        close(mfd);
        return -1;
    }
    snprintf(slave_out, slave_len, "%s", name);

    /* Open the slave once and keep it open in the parent for the
     * lifetime of the daemon. Without a holding open, reads on the
     * master return EIO as soon as the client closes the slave, which
     * makes single-shot SDK calls impossible.
     */
    int sfd = open(slave_out, O_RDWR | O_NOCTTY);
    if (sfd < 0) {
        perror("open(slave)");
        close(mfd);
        return -1;
    }
    struct termios tio;
    if (tcgetattr(sfd, &tio) == 0) {
        tio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
        tio.c_oflag &= ~OPOST;
        tio.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
        tio.c_cflag &= ~(CSIZE|PARENB);
        tio.c_cflag |= CS8|CREAD|CLOCAL;
        tcsetattr(sfd, TCSANOW, &tio);
    }

    *mfd_out = mfd;
    *keepopen_out = sfd;
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    ibutton_t *button = ds1963s_init(ROM);
    if (!button) {
        fprintf(stderr, "ds1963s_init failed\n");
        return 1;
    }
    ds2480_state_t *state = ds2480_init(button);
    if (!state) {
        fprintf(stderr, "ds2480_init failed\n");
        ds1963s_destroy(button);
        return 1;
    }

    int mfd, keepopen;
    char slave[256];
    if (open_pty(&mfd, &keepopen, slave, sizeof(slave)) < 0) {
        ds2480_destroy(state);
        ds1963s_destroy(button);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    struct ctx c = { .mfd = mfd, .keepopen = keepopen, .state = state };
    pthread_t th;
    if (pthread_create(&th, NULL, reader_loop, &c) != 0) {
        perror("pthread_create");
        close(mfd);
        close(keepopen);
        ds2480_destroy(state);
        ds1963s_destroy(button);
        return 1;
    }

    /* Last line wins: stdout, so a launcher script can do
     *   OWPORT=$(./fakeds1963s | tail -n1) ./test1w
     */
    printf("%s\n", slave);
    printf("OWPORT=%s\n", slave);
    fflush(stdout);

    while (!g_stop)
        pause();

    pthread_cancel(th);
    pthread_join(th, NULL);
    close(mfd);
    close(keepopen);
    ds2480_destroy(state);
    ds1963s_destroy(button);
    return 0;
}