#pragma once

#include "extern/ds2480.h"

#if defined(MODULE)
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#define DS_DBG_PRINT(fmt, ...) printk(KERN_INFO "fakeds1963s: " fmt, ##__VA_ARGS__)
#define DS_FREE kfree
#define DS_MALLOC(x) kmalloc((x), GFP_KERNEL)
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define DS_DBG_PRINT(fmt, ...) printf("[FAKEDS1963S] " fmt, ##__VA_ARGS__)
#define DS_FREE free
#define DS_MALLOC(x) malloc((x))
#endif

typedef enum {
    COMMAND = 0,
    DATA
} ds2480_mode_t;

struct _ibutton_t;
typedef struct _ibutton_t ibutton_t;

struct _ds2480_state_t {
    ds2480_mode_t mode;
    unsigned char speed;
    unsigned char baud;
    unsigned char search; // 1 - on, 0 - off
    unsigned char check; // 1 - check mode

    unsigned char search_rom_buffer[16];
    int search_rom_len;
    unsigned char config[256];

    ibutton_t *button;
};
typedef struct _ds2480_state_t ds2480_state_t;

struct _ibutton_t {
    int (*process)(const unsigned char *bytes, size_t count, unsigned char *out, size_t *outsize, int overdrive, ibutton_t *button);
    int (*reset_pulse)(ibutton_t *button);

    void *data;
    unsigned char rom[8];
};


ds2480_state_t *ds2480_init(ibutton_t *button);
int ds2480_process(const unsigned char *bytes, size_t count, unsigned char *out, size_t *outsize, ds2480_state_t *state);
void ds2480_master_reset(ds2480_state_t *state);
void ds2480_soft_reset(ds2480_state_t *state);
void ds2480_destroy(ds2480_state_t *state);
