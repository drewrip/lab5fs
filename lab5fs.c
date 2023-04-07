#include <linux/module.h>
#include <linux/fs.h>
#include <linux/lab5fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>

MODULE_LICENSE("GPL");
static int lab5fs_fill_super(struct super_block *s, void *data, int silent)
{
    // TODO: complete function
}
static struct super_block *lab5fs_get_sb(struct file_system_type *fs_type,
                                         int flags, const char *dev_name, void *data)
{
    return get_sb_bdev(fs_type, flags, dev_name, data, lab5fs_fill_super);
}

/* Define file_system_type for lab5's file system */
static struct file_system_type lab5fs_fs_type = {
    .owner = THIS_MODULE,
    .name = "lab5fs",
    .get_sb = lab5fs_get_sb,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};
/* Module initializer handler */
static int __init init_lab5fs_fs(void)
{
    int res = register_filesystem(&lab5fs_fs_type);
    if (!res)
        printk(KERN_ERR "lab5fs (error): couldn't register lab5fs and got error %d\n", res);
    else
        printk(KERN_INFO "lab5fs (info): successfully registered lab5fs with VFS\n");

    return res;
}
/* Module exit handler */
static int __exit exit_lab5fs_fs(void)
{
    int res = unregister_filesystem(&lab5fs_fs_type);
    if (!res)
        printk(KERN_ERR "lab5fs (error): couldn't unregister lab5fs and got error %d\n", res);
    else
        printk(KERN_INFO "lab5fs (info): successfully unregistered lab5fs from VFS\n");
    return res;
}

module_init(init_lab5fs_fs);
module_exit(exit_lab5fs_fs);
