#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "lab5fs.h"
int validate_image(const char *pathname)
{
    struct stat image_stat;
    int res;
    /* Populate the stat struct from the given image */
    if ((res = stat(pathname, &image_stat)))
    {
        fprintf(stderr, "Could populate stat struct and recevied error number %d\n", res);
        return res;
    }
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
    int fd, res;
    struct timespec now;
    struct lab5fs_super_block sb;
    struct lab5fs_inode root_inode;
    off_t offset = 0;

    // Check arguments
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <device or file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (validate_image(argv[1]))
        exit(EXIT_FAILURE);
    // Open device or file
    fd = open(argv[1], O_WRONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Couldn't open specified device and received error number %d\n", fd);
        exit(EXIT_FAILURE);
    }

    // Fill super block
    memset(&sb, 0, sizeof(sb));
    sb.s_magic = LAB5FS_MAGIC;
    sb.s_block_size = LAB5FS_BSIZE;
    sb.s_inode_size = sizeof(struct lab5fs_inode);
    sb.s_blocks_count = 3;
    sb.s_inodes_count = 5;
    sb.s_free_inodes_count = 4;
    sb.s_free_blocks_count = 2;

    // Write super block to device or file
    if ((res = write(fd, &sb, sizeof(sb))) < 0)
    {
        fprintf(stderr, "Couldn't write super block to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    /* Update offset for next read*/
    offset += res;
    printf("Finished writing %lu bytes to device\n", res);
    /* Initialize root inode */
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    root_inode.i_uid = 0;
    root_inode.i_size = 0;
    now.tv_sec = time(NULL);
    root_inode.i_atime = now.tv_sec;
    root_inode.i_ctime = now.tv_sec;
    root_inode.i_mtime = 0;
    root_inode.i_dtime = 0;
    root_inode.i_gid = 0;
    root_inode.i_links_count = 1;
    root_inode.i_blocks = 1;
    if (lseek(fd, offset, SEEK_SET) < 0)
    {
        fprintf(stderr, "Couldn't return to the right offset in file descriptor\n");
        exit(EXIT_FAILURE);
    }
    // Write root inode to device or file
    if ((res = write(fd, &root_inode, sizeof(root_inode))) < 0)
    {
        fprintf(stderr, "Couldn't write root indoe to device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    if ((res = close(fd)) < 0)
    {
        fprintf(stderr, "Couldn't close device and received error number %d\n", res);
        exit(EXIT_FAILURE);
    }
    printf("lab5fs created on %s\n", argv[1]);

    return 0;
}
