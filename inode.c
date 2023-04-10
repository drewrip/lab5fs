#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include "lab5fs.h"

MODULE_LICENSE("GPL");
void lab5fs_read_inode(struct inode *inode)
{
    unsigned long ino = inode->i_ino;
    struct lab5fs_inode *di;
    struct buffer_head *bh;
    int block, off;
    if (!inode)
    {
        printk(KERN_ERR "lab5fs_read_inode: attempted to read NULL inode\n");
        return;
    }
    block = (ino - LAB5FS_ROOT_INO) / LAB5FS_INODES_PER_BLOCK + 1;
    bh = sb_bread(inode->i_sb, block);
    if (!bh)
    {
        /* yikes */
        printk("couldn't read inode\n");
        return;
    }

    off = (ino - LAB5FS_ROOT_INO) % LAB5FS_INODES_PER_BLOCK;
    di = (struct lab5fs_inode *)bh->b_data + off;

    inode->i_mode = 0x0000FFFF & di->i_mode;
    if (di->i_vtype == LAB5FS_FT_DIR)
    {
        inode->i_mode |= S_IFDIR;
        inode->i_op = &lab5fs_dir_inops;
        inode->i_fop = &lab5fs_dir_operations;
    }
    else if (di->i_vtype == LAB5FS_FT_REG_FILE)
    {
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
int lab5fs_write_inode(struct inode *inode, int wait)
{
    return 0;
}
void lab5fs_delete_inode(struct inode *inode)
{
}