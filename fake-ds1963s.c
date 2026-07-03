/**
 * fakeds1963s.c -- emulation of a DS1963S iButton inside DS2480 serial housing.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/tty_driver.h>
#include "ds2480sim.h"
#include "ds1963s.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick McIlroy");
MODULE_DESCRIPTION("DS1963S iButton emulator over a tty (Linux 7.0+)");
MODULE_VERSION("0.2");

static char *name = "secret";
module_param(name, charp, 0444);
MODULE_PARM_DESC(name, "Location of the system secret");

static struct tty_driver *fake_tty_driver;

static u8 ROM[8] = {0x18,
    '0', '1', '2', '7', '0', '7',
    0x55};

typedef struct {
    struct tty_port port;
    ds2480_state_t *state;
    struct mutex m;
} _fake_serial_info;
static _fake_serial_info *g_serial_info;

static void fakeds1963s_port_shutdown(struct tty_port *port)
{
    if (g_serial_info && g_serial_info->state)
        ds2480_master_reset(g_serial_info->state);
}

static const struct tty_port_operations fakeds1963s_port_ops = {
    .shutdown = fakeds1963s_port_shutdown,
};

static ssize_t fakeds1963s_write(struct tty_struct *tty, const u8 *buffer, size_t count)
{
    int ret;
    unsigned char outbuf[1024];
    size_t pcount = sizeof(outbuf);

    if (!g_serial_info)
        return -ENODEV;

    mutex_lock(&g_serial_info->m);

    ret = ds2480_process(buffer, count, outbuf, &pcount, g_serial_info->state);

    if (ret == -1)
        ret = -ENOMEM;

    if (pcount > 0) {
        tty_insert_flip_string(tty->port, outbuf, pcount);
        tty_flip_buffer_push(tty->port);
    }

    mutex_unlock(&g_serial_info->m);
    return count;
}

static void fakeds1963s_close(struct tty_struct *tty, struct file *filp)
{
    tty_port_close(tty->port, tty, filp);
}

static unsigned int fakeds1963s_write_room(struct tty_struct *tty)
{
    return g_serial_info ? 1024 : 0;
}

static int fakeds1963s_open(struct tty_struct *tty, struct file *filp)
{
    return tty_port_open(tty->port, tty, filp);
}

static const struct tty_operations serial_ops = {
    .open = fakeds1963s_open,
    .close = fakeds1963s_close,
    .write = fakeds1963s_write,
    .write_room = fakeds1963s_write_room,
};

static int __init fakeds1963s_init(void)
{
    int ret;
    ibutton_t *button;

    g_serial_info = kzalloc(sizeof(*g_serial_info), GFP_KERNEL);
    if (!g_serial_info) {
        pr_warn("Could not allocate g_serial_info\n");
        return -ENOMEM;
    }

    mutex_init(&g_serial_info->m);
    tty_port_init(&g_serial_info->port);
    g_serial_info->port.ops = &fakeds1963s_port_ops;

    button = ds1963s_init(ROM);
    if (!button) {
        pr_warn("Could not allocate ds1963s state\n");
        tty_port_destroy(&g_serial_info->port);
        kfree(g_serial_info);
        return -ENOMEM;
    }

    g_serial_info->state = ds2480_init(button);
    if (!g_serial_info->state) {
        pr_warn("Could not allocate ds2480 state\n");
        ds1963s_destroy(button);
        tty_port_destroy(&g_serial_info->port);
        kfree(g_serial_info);
        return -ENOMEM;
    }

    fake_tty_driver = tty_alloc_driver(1,
        TTY_DRIVER_REAL_RAW
        | TTY_DRIVER_RESET_TERMIOS
        | TTY_DRIVER_UNNUMBERED_NODE);
    if (IS_ERR(fake_tty_driver)) {
        ret = PTR_ERR(fake_tty_driver);
        ds2480_destroy(g_serial_info->state);
        ds1963s_destroy(g_serial_info->state->button);
        tty_port_destroy(&g_serial_info->port);
        kfree(g_serial_info);
        return ret;
    }

    fake_tty_driver->driver_name = "fakeds1963s";
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
        pr_err("tty_register_driver failed\n");
        tty_port_destroy(&g_serial_info->port);
        ds1963s_destroy(g_serial_info->state->button);
        ds2480_destroy(g_serial_info->state);
        return ret;
    }

    pr_info("sup\n");
    return 0;
}

static void __exit fakeds1963s_exit(void)
{
    ibutton_t *button = g_serial_info->state->button;

    tty_unregister_driver(fake_tty_driver);
    tty_driver_kref_put(fake_tty_driver);
    tty_port_destroy(&g_serial_info->port);
    ds2480_destroy(g_serial_info->state);
    ds1963s_destroy(button);
    kfree(g_serial_info);
    pr_info("unloaded\n");
}

module_init(fakeds1963s_init);
module_exit(fakeds1963s_exit);
