#include <linux/init.h>             // Macros used to mark up functions e.g., __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/device.h>
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/cdev.h>
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Patrick McIlroy");
MODULE_DESCRIPTION("module sandbox for 4.1");
MODULE_VERSION("0.1");
 
static char *name = "secret";
module_param(name, charp, S_IRUGO);
MODULE_PARM_DESC(name, "Location of the system secret");

static unsigned char system_secret[47];
static unsigned char buffer[2048];

static dev_t fakeds1963s_dev;
static struct cdev fakeds1963s_cdev;


static int fakeds1963s_open(struct inode *inode, struct file *filp) {
    return 0;
}

static ssize_t fakeds1963s_read(struct file *filp, char __user *buff,
    size_t count, loff_t *offp) {

    return 0;
}

static ssize_t fakeds1963s_write(struct file *filp, const char __user *buff,
    size_t count, loff_t *offp) {

    return 0;
}

static int fakeds1963s_close(struct inode *inode, struct file *filp) {
    return 0;
}
 
struct file_operations fakeds1963s_fops = {
    .owner = THIS_MODULE,
    .open = fakeds1963s_open,
    .release = fakeds1963s_close,
    .read = fakeds1963s_read,
    .write = fakeds1963s_write
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init fakeds1963s_init(void) {
    int ret;
    ret = alloc_chrdev_region(&fakeds1963s_dev, 0, 1, "fakeds1963s");
    if (ret != 0) {
        printk(KERN_INFO "Could not allocate major for fakeds1963s, exiting...\n");
        return 1;
    }
    cdev_init(&fakeds1963s_cdev, &fakeds1963s_fops);
    if (cdev_add(&fakeds1963s_cdev, fakeds1963s_dev, 1)) {
        printk(KERN_INFO "Could not add fakeds1963s device, exiting...\n");
        return 1;
    }
    printk(KERN_INFO "fakeds1963s: major is %d", MAJOR(fakeds1963s_dev));
    return 0;
}
 
/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit fakeds1963s_exit(void) {
    unregister_chrdev_region(fakeds1963s_dev, 1);
    printk(KERN_INFO "fakeds1963s: unloaded\n");
}
 
/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(fakeds1963s_init);
module_exit(fakeds1963s_exit);
