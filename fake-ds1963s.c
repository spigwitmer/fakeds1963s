/**
 * fakeds1963s.c -- emulation of a DS1963S iButton inside DS2480 serial housing.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/crc16.h>
#include <linux/crc8.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/tty_driver.h>
#include <asm-generic/ioctls.h>
#include "ds2480.h"
#include "ds1963s.h"
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick McIlroy");
MODULE_DESCRIPTION("module sandbox for 4.1");
MODULE_VERSION("0.1");
 
static char *name = "secret";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "Location of the system secret");

#define DELAY_TIME (HZ * 1)

static struct tty_driver *fake_tty_driver;

// TODO: put this in a state struct of its own geez..
// memory
static u8 nv[0x200], secrets[0x2a4], scratchpad[16];
// registers
static u8 TA1, TA2, ES;

#define ES_AA 0x80
#define ES_PF 0x20

static u8 ROM[8] = {0x00, 
    '0', '1', '2', '7', '0', '7',
    0x18};

static u8 last_command = 0;

typedef enum {
    AWAIT,
    RESPONSE
} _button_state;
static _button_state current_rw_state = AWAIT;

DECLARE_CRC8_TABLE(ds_crc8_table);

typedef struct {
    struct tty_port port;
    int open_count;
    struct mutex m;
    ds2480_state_t state;
} _fake_serial_info;
static _fake_serial_info *g_serial_info = NULL;

static int fakeds1963s_open(struct tty_struct *tty, struct file *filp) {
    tty->driver_data = g_serial_info;

    mutex_lock(&g_serial_info->m);
    ++g_serial_info->open_count;
    printk(KERN_INFO "fakeds1963s: hi, you're customer number: %d...\n", g_serial_info->open_count);
    mutex_unlock(&g_serial_info->m);

    return 0;
}

static int fakeds1963s_write(struct tty_struct *tty, 
              const unsigned char *buffer, int count) {
    int i, ret;
    unsigned char outbuf[1024];
    size_t pcount = 1024;

    if (!g_serial_info) {
        return -ENODEV;
    }
    mutex_lock(&g_serial_info->m);
    printk(KERN_INFO "fakeds1963s: gettin write stuffs\n");

    ////////
    printk(KERN_NOTICE "Written to fakeds1963s: ");
    for(i = 0; i < count; ++i) {
        printk("%x ", buffer[i]);
    }
    printk("\n");
    /////////

    ret = ds2480_process(buffer, count, outbuf, &pcount, &g_serial_info->state);

    ////
    printk(KERN_NOTICE "Read out from fakeds1963s: ");
    for(i = 0; i < pcount; ++i) {
        printk("%x ", outbuf[i]);
    }
    printk("\n");
    ////

    if (ret == -1) {
        ret = -ENOMEM;
    }

    if (pcount > 0) {
        tty_insert_flip_string(tty->port, outbuf, pcount);
    }
    printk(KERN_INFO "fakeds1963s: wrote %lu back\n", pcount);

    mutex_unlock(&g_serial_info->m);
    return count;
}

static void fakeds1963s_close(struct tty_struct *tty, struct file *filp) {
    mutex_lock(&g_serial_info->m);
    printk(KERN_INFO "fakeds1963s: customer number %d leaving...\n", g_serial_info->open_count);
    if (g_serial_info->open_count > 0) {
        --g_serial_info->open_count;
    }
    mutex_unlock(&g_serial_info->m);
}

static int fakeds1963s_write_room(struct tty_struct *tty) {
    if (g_serial_info) {
        return 1024;
    } else {
        return -ENODEV;
    }
}

static const struct tty_operations serial_ops = {
    .open = fakeds1963s_open,
    .close = fakeds1963s_close,
    .write = fakeds1963s_write,
    .write_room = fakeds1963s_write_room
};

static int __init fakeds1963s_init(void) {
    int ret;
    u8 rom_crc;
    ibutton_t *button = kmalloc(sizeof(ibutton_t), GFP_KERNEL);
    ds1963s_init(button);

    g_serial_info = kmalloc(sizeof(_fake_serial_info), GFP_KERNEL);
    if (!g_serial_info) {
        return -ENOMEM;
    }
    mutex_init(&g_serial_info->m);
    g_serial_info->open_count = 0;
    tty_port_init(&g_serial_info->port);
    ds2480_init(&g_serial_info->state, button);

    fake_tty_driver = tty_alloc_driver(1, 
        TTY_DRIVER_REAL_RAW
        |TTY_DRIVER_RESET_TERMIOS
        |TTY_DRIVER_UNNUMBERED_NODE);
    if (IS_ERR(fake_tty_driver)) {
        tty_port_destroy(&g_serial_info->port);
        kfree(g_serial_info);
        return PTR_ERR(fake_tty_driver);
    }
    if (!fake_tty_driver) {
        tty_port_destroy(&g_serial_info->port);
        kfree(g_serial_info);
        return -ENOMEM;
    }
    fake_tty_driver->driver_name = "fakeds1963s";
    fake_tty_driver->magic = TTY_DRIVER_MAGIC;
    fake_tty_driver->owner = THIS_MODULE;
    fake_tty_driver->name = "tty1963s";
    fake_tty_driver->major = 0;
    fake_tty_driver->minor_start = 0;
    fake_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
    fake_tty_driver->subtype = SERIAL_TYPE_NORMAL;
    fake_tty_driver->init_termios = tty_std_termios;
    fake_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD
                            | HUPCL | CLOCAL;
    fake_tty_driver->init_termios.c_oflag = OPOST | OCRNL | ONOCR | ONLRET;
    fake_tty_driver->init_termios.c_lflag = ISIG;
    fake_tty_driver->init_termios.c_ispeed = 9600;
    fake_tty_driver->init_termios.c_ospeed = 9600;
    tty_set_operations(fake_tty_driver, &serial_ops);
    tty_port_link_device(&g_serial_info->port, fake_tty_driver, 0);
    ret = tty_register_driver(fake_tty_driver);
    if (ret) {
        printk(KERN_ERR "fakeds1963s: tty_register_driver failed\n");
        return ret;
    }

    memset(nv, 0, 512);
    memset(secrets, 0, 0x2a4);
    memset(scratchpad, 0, 16);

    /* The first 8 bits of the serial ROM is a CRC8 characters 2-8 */
    crc8_populate_lsb(ds_crc8_table, 0xAC);
    rom_crc = crc8(ds_crc8_table, &ROM[1], 7, CRC8_INIT_VALUE);
    ROM[0] = rom_crc;

    printk(KERN_INFO "fakeds1963s: sup\n");
    return 0;
}
 
static void __exit fakeds1963s_exit(void) {
    tty_unregister_driver(fake_tty_driver);
    put_tty_driver(fake_tty_driver);
    tty_port_destroy(&g_serial_info->port);
    kfree(g_serial_info);
    printk(KERN_INFO "fakeds1963s: unloaded\n");
}
 
module_init(fakeds1963s_init);
module_exit(fakeds1963s_exit);