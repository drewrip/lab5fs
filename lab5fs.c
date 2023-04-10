#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include "lab5fs.h"

MODULE_LICENSE("GPL");
void lab5fs_read_inode(struct inode *inode)
{
}
int lab5fs_write_inode(struct inode *inode, int wait)
{
	return 0;
}
void lab5fs_delete_inode(struct inode *inode)
{
}
static struct inode *lab5fs_alloc_inode(struct super_block *sb)
{
	struct inode *n = NULL;
	return n;
}
static void lab5fs_destroy_inode(struct inode *inode)
{
}
static void lab5fs_put_super(struct super_block *sb)
{
}
static void lab5fs_write_super(struct super_block *sb)
{
}
static struct super_operations lab5fs_sops = {
	.alloc_inode = lab5fs_alloc_inode,
	.destroy_inode = lab5fs_destroy_inode,
	.read_inode = lab5fs_read_inode,
	.write_inode = lab5fs_write_inode,
	.delete_inode = lab5fs_delete_inode,
	.put_super = lab5fs_put_super,
	.write_super = lab5fs_write_super,
};
static int lab5fs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head *bh;
	struct lab5fs_sb_info *sbi;
	struct lab5fs_super_block *lb5_sb;
	struct inode *root_inode;
	printk("lab5fs (debug): inside lab5fs_fill_super.\n");
	/* Initialize lab5fs private info */
	sbi = kmalloc(sizeof(*sbi), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;
	memset(sbi, 0, sizeof(*sbi));
	/* Set blocksize and blocksize_bits */
	if (!sb_set_blocksize(sb, LAB5FS_BSIZE))
	{
		printk(KERN_ERR "lab5fs: blocksize too small for device.\n");
		goto failed_sbi;
	}

	if (!(bh = sb_bread(sb, 0)))
	{
		printk(KERN_ERR "lab5fs: couldn't read superblock from disk.\n");
		goto failed_sbi;
	}
	lb5_sb = (struct lab5fs_super_block *)bh->b_data;
	if (lb5_sb->s_magic != LAB5FS_MAGIC)
	{
		if (!silent)
			printk(KERN_ERR "lab5fs: Couldn't find lab5fs on device %s\n",
				   sb->s_id);
		goto failed_mount;
	}
	sb->s_magic = LAB5FS_MAGIC;
	sb->s_op = &lab5fs_sops;
	// root_inode = iget(sb, 2);
	// if (!root_inode)
	// {
	// 	printk("lab5fs (debug): failed iget.\n");
	// 	goto failed_mount;
	// }
	// sb->s_root = d_alloc_root(root_inode);
	// if (!sb->s_root)
	// {
	// 	printk("lab5fs (debug): failed d_alloc_root.\n");
	// 	iput(root_inode);
	// 	goto failed_mount;
	// }
	printk("lab5fs (debug): leaving lab5fs_fill_super.\n");
	return 0;

/* Handlers for fails */
failed_mount:
	brelse(bh);
	printk("lab5fs (debug): inside failed_mount.\n");

failed_sbi:
	printk("lab5fs (debug): inside failed_sbi.\n");
	sb->s_fs_info = NULL;
	kfree(sbi);
	return -EINVAL;
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
	if (res)
		printk(KERN_ERR "lab5fs (error): couldn't register lab5fs and got error %d\n", res);
	else
		printk(KERN_INFO "lab5fs (info): successfully registered lab5fs with VFS\n");

	return res;
}
/* Module exit handler */
static void __exit exit_lab5fs_fs(void)
{
	int res = unregister_filesystem(&lab5fs_fs_type);
	if (res)
		printk(KERN_ERR "lab5fs (error): couldn't unregister lab5fs and got error %d\n", res);
	else
		printk(KERN_INFO "lab5fs (info): successfully unregistered lab5fs from VFS\n");
}

module_init(init_lab5fs_fs);
module_exit(exit_lab5fs_fs);
