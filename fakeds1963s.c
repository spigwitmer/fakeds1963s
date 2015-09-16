/**
 * fakeds1963s.c -- emulation of a DS1963S iButton inside serial housing.
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
#include "fakeds1963s.h"
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick McIlroy");
MODULE_DESCRIPTION("module sandbox for 4.1");
MODULE_VERSION("0.1");
 
static char *name = "secret";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "Location of the system secret");

DEFINE_SPINLOCK(transaction_lock);

#define DELAY_TIME HZ * 1

static struct tty_driver *fake_tty_driver;
static dev_t fakeds1963s_dev;

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
    struct tty_struct *tty;
    int open_count;
    struct mutex m;
    struct timer_list *timer;
    u8 *buf;
    int buflen;
    int bufsize;
} _fake_serial_info;
static _fake_serial_info *g_serial_info = NULL;

static void fakeds1963s_timer(unsigned long timer_data) {
    _fake_serial_info *f = (_fake_serial_info *)timer_data;
    struct tty_struct *tty;
    struct tty_port *port;
    char *msg;
    int i, msglen;

    tty = f->tty;
    port = tty->port;

    // TODO examine the buffer written to in serial_info, flip it after parsing it

    msg = "DEXTER'S SECRET ";
    msglen = strlen(msg);
    for (i = 0; i < msglen; ++i) {
        // flip one round of our message to core
        if (!tty_buffer_request_room(port, 1))
            tty_flip_buffer_push(port);
        tty_insert_flip_char(port, msg[i], TTY_NORMAL); 
    }

    f->timer->expires = jiffies + DELAY_TIME;
    add_timer(f->timer);
}

static int fakeds1963s_open(struct tty_struct *tty, struct file *filp) {
    g_serial_info->open_count = 0;
    mutex_lock(&g_serial_info->m);
    tty->driver_data = g_serial_info;
    g_serial_info->tty = tty;
    struct timer_list *timer;

    ++g_serial_info->open_count;

    if (g_serial_info->open_count == 1) {
        if (!g_serial_info->timer) {
            timer = kmalloc(sizeof(*timer), GFP_KERNEL);
            if (!timer) {
                mutex_unlock(&g_serial_info->m);
                return -ENOMEM;
            }
            g_serial_info->timer = timer;
        }
        g_serial_info->timer->data = (unsigned long)g_serial_info;
        g_serial_info->timer->expires = jiffies + DELAY_TIME;
        g_serial_info->timer->function = fakeds1963s_timer;
        add_timer(g_serial_info->timer);
    }
    mutex_unlock(&g_serial_info->m);
    return tty_port_open(tty->port, tty, filp);
}

static int fakeds1963s_write(struct tty_struct *tty, 
              const unsigned char *buffer, int count) {
    int i;
    if (!g_serial_info) {
        return -ENODEV;
    }
    mutex_lock(&g_serial_info->m);
    if (g_serial_info->open_count == 0) {
        mutex_unlock(&g_serial_info->m);
        return -EINVAL;
    }
    if (g_serial_info->buflen+count > g_serial_info->bufsize) {
        // expand the buffer
        int newsize = sizeof(u8) * (((g_serial_info->buflen+count)/1024) + 1) * 1024;
        printk(KERN_NOTICE "ds1963s: expanding buffer to %d\n", newsize);
        u8 *newbuf = kmalloc(newsize, GFP_KERNEL);
        if (!newbuf) {
            mutex_unlock(&g_serial_info->m);
            return -ENOMEM;
        }
        memcpy(newbuf, g_serial_info->buf, g_serial_info->buflen);
        g_serial_info->bufsize = newsize;
        g_serial_info->buf = newbuf;
    }
    memcpy(&g_serial_info->buf[g_serial_info->buflen], buffer, count);
    g_serial_info->buflen += count;
    printk(KERN_NOTICE "Written to fakeds1963s: ");
    
    mutex_unlock(&g_serial_info->m);
    return min(count, 1024);
}

static void fakeds1963s_close(struct tty_struct *tty, struct file *filp) {
    mutex_lock(&g_serial_info->m);
    if (g_serial_info->open_count > 0) {
        --g_serial_info->open_count;
    }
    mutex_unlock(&g_serial_info->m);
    tty_port_close(tty->port, tty, filp);
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

    g_serial_info = kmalloc(sizeof(_fake_serial_info), GFP_KERNEL);
    if (!g_serial_info) {
        return -ENOMEM;
    }
    mutex_init(&g_serial_info->m);
    g_serial_info->buf = kmalloc(1024 * sizeof(u8), GFP_KERNEL);
    g_serial_info->bufsize = 1024;
    g_serial_info->buflen = 0;

    ret = alloc_chrdev_region(&fakeds1963s_dev, 0, 1, "fakeds1963s");
    if (ret != 0) {
        printk(KERN_INFO "Could not allocate major for fakeds1963s, exiting...\n");
        return 1;
    }
    printk(KERN_INFO "fakeds1963s: major is %d", MAJOR(fakeds1963s_dev));

    fake_tty_driver = tty_alloc_driver(1, 
        TTY_DRIVER_REAL_RAW|TTY_DRIVER_UNNUMBERED_NODE|TTY_DRIVER_RESET_TERMIOS);
    if (!fake_tty_driver) {
        unregister_chrdev_region(fakeds1963s_dev, 1);
        kfree(g_serial_info);
        return -ENOMEM;
    }
    fake_tty_driver->driver_name = "ttyfakeds1963s";
    fake_tty_driver->magic = TTY_DRIVER_MAGIC;
    fake_tty_driver->owner = THIS_MODULE;
    fake_tty_driver->name = "tty1963s";
    fake_tty_driver->major = MAJOR(fakeds1963s_dev);
    fake_tty_driver->minor_start = 3;
    fake_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
    fake_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	fake_tty_driver->init_termios = tty_std_termios;
	fake_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD
							| HUPCL | CLOCAL;
	fake_tty_driver->init_termios.c_ispeed = 9600;
	fake_tty_driver->init_termios.c_ospeed = 9600;
	tty_set_operations(fake_tty_driver, &serial_ops);

    ret = tty_register_driver(fake_tty_driver);
    if (ret) {
        printk(KERN_ERR "fakeds1963s: tty_register_driver failed\n");
        unregister_chrdev_region(fakeds1963s_dev, 1);
        return ret;
    }
    tty_register_device(fake_tty_driver, 0, NULL);

    memset(nv, 0, 512);
    memset(secrets, 0, 0x2a4);
    memset(scratchpad, 0, 16);

    /* The first 8 bits of the serial ROM is a CRC8 characters 2-8 */
    crc8_populate_lsb(ds_crc8_table, 0xAC);
    rom_crc = crc8(ds_crc8_table, &ROM[1], 7, CRC8_INIT_VALUE);
    ROM[0] = rom_crc;
    return 0;
}
 
static void __exit fakeds1963s_exit(void) {
    tty_unregister_device(fake_tty_driver, 0);
    tty_unregister_driver(fake_tty_driver);
    unregister_chrdev_region(fakeds1963s_dev, 1);
    kfree(g_serial_info);
    printk(KERN_INFO "fakeds1963s: unloaded\n");
}
 
module_init(fakeds1963s_init);
module_exit(fakeds1963s_exit);
