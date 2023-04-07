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
    return 0;
}
static struct super_block *lab5fs_get_sb(struct file_system_type *fs_type,
                                         int flags, const char *dev_name, void *data)
{
    return get_sb_bdev(fs_type, flags, dev_name, data, lab5fs_fill_super);
}

static void lab5fs_read_inode(struct inode* inode){
	unsigned long ino = inode->i_ino;
	struct lab5fs_inode* di;
	struct buffer_head* bh;
	int block, off;
	
	blok = (ino - LAB5FS_ROOT_INO)/LAB5FS_INODES_PER_BLOCK + 1;
	bh = sb_bread(inode->i_sb, block);
	if(!bh){
		/* yikes */
		printf("couldn't read inode\n");
		return;
	}
	
	off = (ino - LAB5FS_ROOT_INO) % LAB5FS_INODES_PER_BLOCK;
	di = (struct lab5fs_inode*)bh->b_data + off;

	inode->i_mode = 0x0000FFFF & di->i_mode;
	if(di->i_vtype == LAB5FS_FT_DIR){
		inode->i_mode |= S_IFDIR;
		inode->i_op = &lab5fs_dir_inops;
		inode->i_fop = &lab5fs_dir_operations;
	} else if (di->i_vtype == LAB5FS_FT_REG_FILE){
		inode->i_mode |= S_IFREG;
		inode->i_op = &lab5fs_file_inops;
		inode->i_fop = &lab5fs_file_operations;
		inode->i_mapping->a_ops = &lab5fs_aops;
	}	

	inode->i_uid = di->i_uid;
	inode->i_gid = di->i_gid;
	inode->i_size = LAB5FS_FILESIZE(di);
	inode->i_blocks = LAB5FS_FILEBLOCKS(di);
	inode->i_atime.tv_sec = di->i_atime;
	inode->i_mtime.tv_sec = di->i_mtime;
	inode->i_ctime.tv_sec = di->i_ctime;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_nsec = 0;	

	brelse(bh);
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
