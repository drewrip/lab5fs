#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "lab5fs.h"

int validate_image(const char *pathname, unsigned long *block_count)
{
    struct stat image_stat;
    int res;
    /* Populate the stat struct from the given image */
    if ((res = stat(pathname, &image_stat)))
    {
        fprintf(stderr, "Could populate stat struct and recevied error number %d\n", res);
        return res;
    }
    /* Let's get the number blocks in the device */
    *block_count = (image_stat.st_size / LAB5FS_BSIZE) - 3;
    if (!(*block_count))
        return -1;
    /* Ensure image has right permissions */
    if (res = access(pathname, W_OK))
    {
        fprintf(stderr, "Device isn't writeable\n");
        return res;
    }
    return 0;
}
int main(int argc, char *argv[])
{
    int fd, res, cmp;
    unsigned long block_count, max_num_of_inodes;
    struct timespec now;
    struct lab5fs_super_block sb;
    struct lab5fs_inode root_inode;
    double block_ino_ratio = (double)sizeof(struct lab5fs_inode) / (double)sizeof(struct lab5fs_dir_entry);
    off_t offset = 0;

    // Check arguments
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <device or file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    /* Validate image/device and obtain the number of blocks in the image */
    if (validate_image(argv[1], &block_count))
        exit(EXIT_FAILURE);
    /* Obtain the max number of nodes for this device.
     * Will need to change block_count - 3
     * based on how many blocks are used for other purposes than inodes */
    max_num_of_inodes = (int)((double)block_count * block_ino_ratio);
    fprintf(stdout, "max num of inodes is %lu and block_count is %lu\n", max_num_of_inodes, block_count);
    // Open device or file
    fd = open(argv[1], O_WRONLY | O_EXCL);
    if (fd < 0)
    {
        fprintf(stderr, "Couldn't open specified device and received error number %d\n", fd);
        exit(EXIT_FAILURE);
    }

    // Fill super block
    memset(&sb, 0, sizeof(sb));
    sb.s_magic = LAB5FS_MAGIC;
    sb.s_block_size = LAB5FS_BSIZE;
    sb.s_blocksize_bits = LAB5FS_BSIZE_BITS;
    sb.s_inode_size = sizeof(struct lab5fs_inode);
    sb.s_blocks_count = block_count;
    sb.s_inodes_count = max_num_of_inodes;
    sb.s_free_inodes_count = max_num_of_inodes - 1;
    sb.s_free_blocks_count = block_count;
    sb.s_first_data_block = max_num_of_inodes + INODE_TABLE_BLOCK_NO - 1;
    lab5fs_bitmap data_block_bitmap, inode_bitmap;
    struct lab5fs_dir_entry dentry;
    fprintf(stdout, "sb.s_first_data_block is %lu\n", sb.s_first_data_block);
    // Write super block to device or file
    if ((res = write(fd, &sb, sizeof(sb))) < 0)
    {
        fprintf(stderr, "Couldn't write super block to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    /* Update offset for next write*/
    offset += res;
    printf("Finished writing %lu bytes to device's superblock\n", res);
    /*  Initialize and write data block bitmap to device */
    /*  We need to ensure that data block bitmap can map all data blocks,
     *  so perform check LAB5FS_BLOCK_SIZE * 8 < TOTAL_NUM_OF_DATA_BLOCKS
     *  TODO: account for number of data blocks greater than LAB5FS_BLOCK_SIZE * 8
     *  and factor-in the number of blocks needed for the inode-table
     */
    /* Set first bit for root inode '.' & '..' entries */
    data_block_bitmap.bitmap[0] |= 0x3;

    memset(&data_block_bitmap, 0, sizeof(data_block_bitmap));
    if ((res = write(fd, &data_block_bitmap, sizeof(data_block_bitmap))) < 0)
    {
        fprintf(stderr, "Couldn't write data block bitmap to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    /* Update offset for next write*/
    offset += res;
    printf("Finished writing %lu bytes to device's data block bitmap\n", res);
    /* Initialize and write inode bitmap to device */
    memset(&inode_bitmap, 0, sizeof(inode_bitmap));
    /* Set the first two bits of the inode_bitmap for the root inode and NULL node */
    inode_bitmap.bitmap[0] |= 0x3;
    if ((res = write(fd, &inode_bitmap, sizeof(inode_bitmap))) < 0)
    {
        fprintf(stderr, "Couldn't write inode bitmap to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    /* Update offset for next write*/
    offset += res;
    printf("Finished writing %lu bytes to device's inode bitmap\n", res);
    /* Initialize root inode */
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    root_inode.i_uid = 0;
    // root_inode.i_size = LAB5FS_BSIZE; // default size
    now.tv_sec = time(NULL);
    root_inode.i_atime = now.tv_sec;
    root_inode.i_ctime = now.tv_sec;
    root_inode.i_mtime = 0;
    root_inode.i_dtime = 0;
    root_inode.i_gid = 0;
    root_inode.i_ino = LAB5FS_ROOT_INODE;
    root_inode.i_links_count = 2;
    root_inode.i_blocks = 1;
    if ((cmp = lseek(fd, offset, SEEK_SET)) < 0 || cmp != offset)
    {
        fprintf(stderr, "Couldn't return to the right offset in file descriptor\n");
        exit(EXIT_FAILURE);
    }
    // Write root inode to device or file
    if ((res = write(fd, &root_inode, sizeof(root_inode))) < 0)
    {
        fprintf(stderr, "Couldn't write root inode to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    /* Update offset for next write*/
    offset += res;
    printf("Finished writing %lu bytes to device's root inode\n", res);
    /* Update offset to write in data blocks, so skip inode tables */
    offset += (LAB5FS_BSIZE - res) + (max_num_of_inodes - 2) * LAB5FS_BSIZE;
    printf("New offset is %lu\n", offset);
    if ((cmp = lseek(fd, offset, SEEK_SET)) < 0 || cmp != offset)
    {
        fprintf(stderr, "Couldn't return to the right offset in file descriptor\n");
        exit(EXIT_FAILURE);
    }
    memset(&dentry, 0, sizeof(dentry));
    dentry.inode = 1;
    dentry.namelen = strlen(".") + 1;
    dentry.rec_len = REC_LEN_ALIGN_FOUR(dentry.namelen);
    dentry.file_type = 2;
    strcpy(dentry.name, ".");

    // Write root dentry to device or file
    if ((res = write(fd, &dentry, dentry.rec_len)) < 0)
    {
        fprintf(stderr, "Couldn't write root dentry '.' to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    /* Update offset for next write*/
    offset += res;
    printf("Finished writing %lu bytes to device's root dentry '.'\n", res);
    if ((cmp = lseek(fd, offset, SEEK_SET)) < 0 || cmp != offset)
    {
        fprintf(stderr, "Couldn't return to the right offset in file descriptor\n");
        exit(EXIT_FAILURE);
    }
    memset(&dentry, 0, sizeof(dentry));
    dentry.inode = 1;
    dentry.namelen = strlen("..") + 1;
    dentry.rec_len = REC_LEN_ALIGN_FOUR(dentry.namelen);
    dentry.file_type = 2;
    strcpy(dentry.name, "..");

    // Write root dentry to device or file
    if ((res = write(fd, &dentry, dentry.rec_len)) < 0)
    {
        fprintf(stderr, "Couldn't write root indoe dentry '..' to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    printf("Finished writing %lu bytes to device's root dentry '..'\n", res);
    /* Update offset for next write*/
    offset += res;
    if ((res = close(fd)) < 0)
    {
        fprintf(stderr, "Couldn't close device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    printf("lab5fs created on %s\n", argv[1]);

    return 0;
}
