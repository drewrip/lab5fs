#ifndef _LAB5FS_FS_H
#define _LAB5FS_FS_H
#define LAB5FS_BSIZE_BITS 10
#define LAB5FS_BSIZE (1 << LAB5FS_BSIZE_BITS) /* 1024 bytes per block*/
#define LAB5FS_INODES_PER_BLOCK 8
#define LAB5FS_MAGIC 0xCAFED00D
#define LAB5FS_ROOT_INO 1
#include <linux/types.h>

/* lab5fs superblock layout on disk */
struct lab5fs_super_block
{
    unsigned long s_magic;             /* Magic signature */
    unsigned long s_inodes_count;      /* Inodes count */
    unsigned long s_blocks_count;      /* Blocks count */
    unsigned long s_free_blocks_count; /* Free blocks count */
    unsigned long s_free_inodes_count; /* Free inodes count */
    unsigned long s_first_data_block;  /* First Data Block */
    unsigned long s_block_size;        /* Block size */

    unsigned short s_state;                                                                   /* File system state */
    unsigned short s_inode_size;                                                              /* size of inode structure */
    char s_reserved[LAB5FS_BSIZE - (7 * sizeof(unsigned long) + 2 * sizeof(unsigned short))]; /* Padding to 1024*/
};
/* lab5fs inode layout on disk */
struct lab5fs_inode
{
    unsigned short i_mode;        /* File mode */
    unsigned short i_uid;         /* Low 16 bits of Owner Uid */
    unsigned long i_size;         /* Size in bytes */
    unsigned long i_atime;        /* Access time */
    unsigned long i_ctime;        /* Creation time */
    unsigned long i_mtime;        /* Modification time */
    unsigned long i_dtime;        /* Deletion Time */
    unsigned short i_gid;         /* Low 16 bits of Group Id */
    unsigned short i_links_count; /* Links count */
    unsigned long i_blocks;       /* Blocks count */
    unsigned long i_flags;        /* File flags */
    /* 36 bytes up till this point */
    char padding[28]; /* pad up to 64 bytes */
};
/* lab5fs dentry layout on disk */
struct lab5fs_dir_entry
{
    unsigned long inode;    /* Inode number */
    unsigned short rec_len; /* Directory entry length */
    char *name_len;         /* Name length */
    char *file_type;
    char name[255]; /* File name */
};
enum
{
    LAB5FS_FT_UNKNOWN,
    LAB5FS_FT_REG_FILE,
    LAB5FS_FT_DIR,
};
#define BFS_FILESIZE(ip) \
    ((ip)->i_sblock == 0 ? 0 : BFS_NZFILESIZE(ip))

#define BFS_FILEBLOCKS(ip) \
    ((ip)->i_sblock == 0 ? 0 : ((ip)->i_eblock + 1) - (ip)->i_sblock)

struct lab5fs_sb_info
{
};
/* super.c */

/* inode.c */

#endif
