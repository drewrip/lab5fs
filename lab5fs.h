#ifndef _LAB5FS_FS_H
#define _LAB5FS_FS_H
#define LAB5FS_BSIZE_BITS 10
#define LAB5FS_BSIZE (1 << LAB5FS_BSIZE_BITS) /* 1024 bytes per block*/
#define LAB5FS_INODES_PER_BLOCK 8
#define LAB5FS_MAGIC 0xCAFED00D
#define LAB5FS_ROOT_INO 1
#define INODE_TABLE_BLOCK_NO 3
#define INODE_BITMAP_BLOCK_NO 2
#define MAX_FILE_NAME_LENGTH 255
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
/*
 * lab5fs in-memory super_block info
 */
struct lab5fs_sb_info
{
    unsigned long *i_sblock_dentries; /* Starting block of dir entries */
    lab5fs_bitmap *inode_bitmap;      /* Keep pointer to inode bitmap in memory */
    buffer_head *inode_bitmap_bh;
    unsigned long sbi_last_ino;
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
/*
 * lab5fs in-memory inode info
 */
struct lab5fs_inode_info
{
    unsigned long i_sblock_dentries; /* Starting block of dir entries */
    unsigned long i_sblock_data;     /* Starting block of dir entries */
    unsigned long i_eblock_data;     /* Starting block of dir entries */
};
/* lab5fs dentry layout on disk */
struct lab5fs_dir_entry
{
    unsigned long inode;    /* Inode number */
    unsigned short rec_len; /* Directory entry length */
    int namelen;            /* Name length */
    char *file_type;
    char name[MAX_FILE_NAME_LENGTH]; /* File name */
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

/* Define bitmap structure for lab5fs */
typedef struct
{
    unsigned long bitmap[LAB5FS_BSIZE >> 2];
} lab5fs_bitmap;
/* Define block allocation bitmap structure for dir entries in lab5fs */
typedef struct
{
    unsigned long block_map[LAB5FS_BSIZE >> 2]; /* Maps 256 inodes in one block */
} lab5fs_block_map;
/* super.c */

/* inode.c */

#endif
