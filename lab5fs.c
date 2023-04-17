#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include "lab5fs.h"

/* Declare static functions for file system operations */
static int lab5fs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd);
static int lab5fs_link(struct dentry *old_dentry, struct inode *dir,
					   struct dentry *dentry);
static int lab5fs_unlink(struct inode *dir, struct dentry *dentry);
static struct dentry *lab5fs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
static int lab5fs_readdir(struct file *flip, void *dirent, filldir_t filldir);

MODULE_LICENSE("GPL");
static int lab5fs_add_entry(struct dentry *dentry, struct inode *inode)
{
	printk("lab5fs_add_entry (debug): inside lab5fs_add_entry\n");
	struct buffer_head *bh_dir;
	struct lab5fs_inode_info *in_info;
	struct lab5fs_dir_entry *current_lab5fs_de;
	int offset = 0, block_no, data_sblock_no, data_eblock_no;
	struct inode *parent_inode = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	unsigned short current_de_rec_len = 0, current_de_aligned_rec_len,
				   /* Get the rec_len of the future entry 4 bytes aligned */
		future_de_rec_len = REC_LEN_ALIGN_FOUR(namelen);
	in_info = inode->u.generic_ip;
	data_sblock_no = in_info->i_sblock_data;
	data_eblock_no = in_info->i_eblock_data;

	if (!namelen)
	{
		printk("lab5fs_add_entry: entry doesn't exist\n");
		return -ENOENT;
	}
	if (namelen > MAX_FILE_NAME_LENGTH)
		printk("lab5fs_add_entry: entry name exceed max name length\n");
	return -ENAMETOOLONG;
	/* Iterate through data blocks until we find a free chunk */
	for (block_no = data_sblock_no; block_no <= data_eblock_no; block_no++)
	{
		/* Read the block from disk */
		bh_dir = sb_bread(inode->i_sb, block_no);
		if (!bh_dir)
		{
			printk("lab5fs_add_entry: couldn't find space for dentry\n");
			return -ENOSPC;
		}
		/* If we get here, that means that there might be space, so we look for a free chunk within the block */
		for (; offset < LAB5FS_BSIZE; offset += current_de_rec_len)
		{
			/* Let's obtain thet lab5fs dentry from disk to check if it's empty */
			current_lab5fs_de = (struct lab5fs_dir_entry *)(bh_dir->b_data + offset);
			/* If inode number isn't set and the chunk's size can accomodate out entry, we presist the data */
			current_de_rec_len = current_lab5fs_de->rec_len;
			if (!current_lab5fs_de->inode && current_de_rec_len >= future_de_rec_len)
				goto found_chunk;
			/*  If the last statement didn't pass, don't panic, we can still find space.
			 *  Let's see if at least the curent entry has some padding that can fit our entry.
			 */
			current_de_aligned_rec_len = REC_LEN_ALIGN_FOUR(current_lab5fs_de->namelen);
			if (current_de_rec_len >= future_de_rec_len + current_de_aligned_rec_len)
				goto found_chunk;
		}
		brelse(bh_dir);
	}
	/* Uh no, there's no space */
	printk("leaving lab5fs_add_entry after finding no space\n");
	return -ENOSPC;

found_chunk:
	current_lab5fs_de->rec_len = current_de_aligned_rec_len;
	/* Handle case where we're adding the dentry into the chunk of another dentry */
	if (current_lab5fs_de->inode)
	{
		struct lab5fs_dir_entry *tmp_de = (struct lab5fs_dir_entry *)((char *)(current_de_rec_len + current_lab5fs_de->namelen));
		tmp_de->rec_len = current_de_aligned_rec_len - current_lab5fs_de->namelen;

		current_lab5fs_de = tmp_de;
	}
	current_lab5fs_de->inode = inode->i_ino;
	current_lab5fs_de->namelen = namelen;
	memcpy(current_lab5fs_de->name, name, namelen);

	/* Update parent inode */
	parent_inode->i_size += current_lab5fs_de->rec_len;
	parent_inode->i_mtime = parent_inode->i_ctime = CURRENT_TIME;
	/* Mark inode and buffer dirty and return the caller */
	mark_inode_dirty(parent_inode);
	mark_buffer_dirty(bh_dir);
	brelse(bh_dir);
	printk("leaving lab5fs_add_entry after finding space\n");
	return 0;
}
static int lab5fs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	printk("lab5fs_create (debug): inside lab5fs_create\n");
	int res;
	struct inode *inode;
	struct super_block *sb;
	struct lab5fs_sb_info *sb_info;
	struct lab5fs_inode_info *in_info;
	struct buffer_head *bh_bitmap;
	struct lab5fs_bitmap *b_map;
	unsigned long ino;
	printk("lab5fs_create (debug): passed definition stage\n");
	if (!dir)
	{
		printk("lab5fs_create: attempted to create an entry for a NULL inode\n");
		return -EFAULT;
	}
	printk("lab5fs_create (debug): dir is not NULL but is %p\n", dir);
	sb = dir->i_sb;
	printk("lab5fs_create (debug): passed 1 assignment stage\n");
	sb_info = sb->s_fs_info;
	printk("lab5fs_create (debug): passed 2 assignment stages\n");
	bh_bitmap = sb_info->inode_bitmap_bh;
	printk("lab5fs_create (debug): passed 3 assignment stages\n");
	b_map = ((struct lab5fs_bitmap *)(bh_bitmap->b_data));
	printk("lab5fs_create (debug): passed all assignment stages\n");
	inode = new_inode(sb);
	if (!inode)
		return -ENOSPC;
	in_info = kmalloc(sizeof(struct lab5fs_inode_info), GFP_KERNEL);
	if (!in_info)
	{
		iput(inode);
		printk("lab5fs_create: couldn't find space to allocate lab5fs_sb_info\n");
		return -ENOSPC;
	}
	/* Update file offset in the event that the block read failed */
	if (!bh_bitmap)
	{
		printk("lab5fs_create: couldn't read inode bitmap\n");
		return -1;
	}
	printk("lab5fs_create (debug): passed bitmap block stage\n");
	ino = find_first_zero_bit(b_map->bitmap, LAB5FS_BSIZE);
	printk("lab5fs_create (debug): passed bitmap block bit set\n");
	if (ino > LAB5FS_BSIZE)
	{
		iput(inode);
		return -ENOSPC;
	}
	// TODO: after finding the block number, we persist it to data, set the bit, and add the entry
	// look into insert_into hash and mark dirty
	// look into locking super block
	set_bit(ino, b_map->bitmap);
	mark_buffer_dirty(bh_bitmap);
	inode->i_mode = mode;
	inode->i_ino = ino;
	inode->i_size = 0;
	inode->i_blocks = 0;

	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &lab5fs_file_inops;
	inode->i_fop = &lab5fs_dir_operations;
	inode->i_mapping->a_ops = &lab5fs_aops;
	inode->i_nlink = 1;
	inode->u.generic_ip = in_info;
	printk("lab5fs_create (debug): passed inode assignment stage 1\n");
	insert_inode_hash(inode);
	mark_inode_dirty(inode);
	/* Add dentry to data block to establish the inode link */
	res = lab5fs_add_entry(dentry, inode);
	if (res)
	{
		inode->i_nlink--;
		mark_inode_dirty(inode);
		iput(inode);
		printk("lab5fs_create: couldn't add dir entry and received error number %d\n", res);
		return res;
	}
	d_instantiate(dentry, inode);
	printk("lab5fs_create (debug): passed inode assignment stage 2\n");
	// set buffer to dirty
	printk("lab5fs_create (debug): leaving lab5fs_create\n");
	return 0;
}
static int lab5fs_unlink(struct inode *dir, struct dentry *dentry)
{
	/** TODO: Complete function*/
	printk("Inside lab5fs_unlink\n");
	return 0;
}
static int lab5fs_link(struct dentry *old_dentry, struct inode *dir,
					   struct dentry *dentry)
{
	/** TODO: Complete function*/
	printk("Inside lab5fs_link\n");
	return 0;
}
static struct buffer_head *lab5fs_find_entry(struct inode *dir, const char *name, int namelen, struct lab5fs_dir_entry **res_dir)
{
	unsigned long block, offset;
	struct buffer_head *bh_dir = NULL;
	struct lab5fs_dir_entry *de;
	struct lab5fs_inode_info *in_info = (struct lab5fs_inode_info *)(dir->u.generic_ip);
	printk("lab5fs_find_entry: inside lab5fs_find_entry\n");
	*res_dir = NULL;
	if (namelen > MAX_FILE_NAME_LENGTH)
		return NULL;
	bh_dir = NULL;
	block = offset = 0;
	while (block * LAB5FS_BSIZE + offset < dir->i_size)
	{
		if (!bh_dir)
		{
			bh_dir = sb_bread(dir->i_sb, in_info->i_sblock_data + block);
			if (!bh_dir)
			{
				block++;
				continue;
			}
		}
		de = (struct lab5fs_dir_entry *)(bh_dir->b_data + offset);
		if (de && de->rec_len)
			offset += de->rec_len;
		else
			offset += sizeof(struct lab5fs_dir_entry);
		if (de->inode && !memcmp(name, de->name, namelen))
		{
			printk("lab5fs_find_entry: found dentry for inode %lu and name %s\n", de->inode, name);
			*res_dir = de;
			return bh_dir;
		}
		if (offset < bh_dir->b_size)
			continue;
		brelse(bh_dir);
		bh_dir = NULL;
		offset = 0;
		block++;
	}
	brelse(bh_dir);
	/* If we get here, it means we couldn't find the entry */
	printk("lab5fs_find_entry: leaving lab5fs_find_entry\n");
	return NULL;
}
static struct dentry *lab5fs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct buffer_head *bh_dir;
	struct lab5fs_dir_entry *lab5_de;
	struct inode *inode_of_dir = NULL;

	printk("Inside lab5fs_lookup\n");
	if (!dir || !dentry || !dentry->d_name.len)
	{
		printk("lab5fs_lookup: dentry doesn't exist!\n");
		return ERR_PTR(-EINVAL);
	}
	if (dentry->d_name.len > MAX_FILE_NAME_LENGTH)
	{
		printk("lab5fs_lookup: dentry's name exceeds max limit!\n");
		return ERR_PTR(-ENAMETOOLONG);
	}
	/* Find entry and read it into the buffer_head */
	bh_dir = lab5fs_find_entry(dir, dentry->d_name.name, dentry->d_name.len, &lab5_de);
	/* Confirm that the read was successful and proceed to inode extraction */
	if (bh_dir)
	{
		/* We don't need buffer head anymore, so release resources */
		brelse(bh_dir);
		/* Let's get the corresponding inode and confirm that the dentry is valid */
		inode_of_dir = iget(dir->i_sb, lab5_de->inode);
		if (!inode_of_dir)
		{
			printk("lab5fs_lookup: couldn't access inode for corresponding dentry\n");
			return ERR_PTR(-EACCES);
		}
	}
	/* If we get here, the dentry is valid and confirm, so we add to the dcache */
	printk("lab5fs_lookup: calling d_add on inode %p, name %s, and length %d\n", inode_of_dir, dentry->d_name.name, dentry->d_name.len);
	d_add(dentry, inode_of_dir);
	/* Return NULL to idicate that all went well! */
	printk("Leaving lab5fs_lookup\n");
	return NULL;
}

static int lab5fs_readdir(struct file *flip, void *dirent, filldir_t filldir)
{
	printk("lab5fs_readdir (debug): inside lab5fs_readdir\n");
	struct inode *d_ino = flip->f_dentry->d_inode;
	struct buffer_head *bh;
	struct lab5fs_dir_entry *lab5fs_dentry;
	struct lab5fs_inode_info *in_info;
	unsigned int b_offset = 0;
	loff_t f_pos = flip->f_pos;
	unsigned long block_no;
	in_info = (struct lab5fs_inode_info *)(d_ino->u.generic_ip);
	printk("Inside lab5fs_readdir where f_pos is %lld and d_ino->i_size is %llu\n", f_pos, d_ino->i_size);
	/* Ensure that we don't read past the boundary of a dir entry */
	if (flip->f_pos & (sizeof(struct lab5fs_dir_entry) - 1))
	{
		printk("lab5fs_readdir: attempted to read beyond file\n");
		return -1;
	}
	// if (f_pos == 0)
	// {
	// 	filldir(dirent, ".", 1, flip->f_pos, d_ino->i_ino, DT_DIR);
	// 	flip->f_pos++;
	// }

	// if (f_pos == 1)
	// {
	// 	filldir(dirent, "..", 2, flip->f_pos, d_ino->i_ino, DT_DIR);
	// 	flip->f_pos++;
	// }
	/* Start reading the dentries corresponding to the inode */
	while (flip->f_pos < d_ino->i_size)
	{
		/* Calculate offset within a lab5fs block */
		b_offset = flip->f_pos & (LAB5FS_BSIZE - 1);

		/* Obtain starting block number for dentries */
		block_no = in_info->i_sblock_dentries;
		printk("block no is %lu\n", block_no);
		/* Let's read the block and begin dir reading */
		bh = sb_bread(d_ino->i_sb, block_no);
		/* Update file offset in the event that the block read failed */
		if (!bh)
		{
			flip->f_pos += LAB5FS_BSIZE - b_offset;
			continue;
		}
		/*  If we get here, it means that we can start performing the dir read.
		 *  we also need to account for the possibility that the entries span over multiple blocks, so
		 *  we perform the read over one block and go back to outer loop.
		 */
		do
		{
			lab5fs_dentry = (struct lab5fs_dir_entry *)(bh->b_data + b_offset);
			/* Ensure that we don't read the empty dentries */
			if (lab5fs_dentry->inode && lab5fs_dentry->rec_len)
			{
				// printk("lab5fs_readdir: calling filldir on name %s and inode %lu\n", lab5fs_dentry->name, lab5fs_dentry->inode);
				if (filldir(dirent, lab5fs_dentry->name, lab5fs_dentry->namelen, flip->f_pos, lab5fs_dentry->inode, DT_UNKNOWN) < 0)
				{
					brelse(bh);
					return 0;
				}
				b_offset += lab5fs_dentry->rec_len;
				flip->f_pos += lab5fs_dentry->rec_len;
				printk("lab5fs_readdir: done calling filldir and for inode %lu, name %s, namelen is %c, and rec_len %hu\n", lab5fs_dentry->inode, lab5fs_dentry->name, lab5fs_dentry->namelen, lab5fs_dentry->rec_len);
			}
			else
			{
				printk("lab5fs_readdir: skipped filldir with inode %lu\n", lab5fs_dentry->inode);
				b_offset += sizeof(lab5fs_dentry);
				flip->f_pos += sizeof(lab5fs_dentry);
			}

		} while (b_offset < LAB5FS_BSIZE && flip->f_pos < d_ino->i_size);
		brelse(bh);
	}
	printk("Existing lab5fs_readdir\n");
	return 0;
}

static struct address_space_operations lab5fs_aops = {

};
/** TODO: Clean up function and add/remove logic as needed */
void lab5fs_read_inode(struct inode *inode)
{
	printk("lab5fs_read_inode (debug): reading inode\n");
	unsigned long ino = inode->i_ino;
	struct lab5fs_inode *di;
	struct buffer_head *bh;
	struct lab5fs_inode_info *in_info;
	int block, off;

	if (!inode)
	{
		printk("lab5fs_read_inode: attempted to read NULL inode\n");
		return;
	}
	block = (ino - LAB5FS_ROOT_INO) / LAB5FS_INODES_PER_BLOCK + 1;
	/* Get corrersponding block number to the inode number so that we read from the right place */
	bh = sb_bread(inode->i_sb, ((ino * sizeof(struct lab5fs_inode)) / LAB5FS_BSIZE) + INODE_TABLE_BLOCK_NO);
	if (!bh)
	{
		printk(KERN_ERR "lab5fs_read_inode: sb_bread returned null for block sector\n");
		return;
	}

	off = (ino - LAB5FS_ROOT_INO) % LAB5FS_INODES_PER_BLOCK;
	di = (struct lab5fs_inode *)bh->b_data;

	inode->i_mode = di->i_mode;
	printk("lab5fs_read_inode: inode->i_mode is 0x%x, di->i_mode is 0x%x, and initialized mode is 0x%x\n", inode->i_mode, di->i_mode, S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	/**** TODO: add type for inode*/

	// if (di->i_vtype == LAB5FS_FT_DIR)
	// {
	// 	inode->i_mode |= S_IFDIR;
	// 	inode->i_op = &lab5fs_dir_inops;
	// 	inode->i_fop = &lab5fs_dir_operations;
	// }
	// else if (di->i_vtype == LAB5FS_FT_REG_FILE)
	// {
	// 	inode->i_mode |= S_IFREG;
	// 	inode->i_op = &lab5fs_file_inops;
	// 	inode->i_fop = &lab5fs_file_operations;
	// 	inode->i_mapping->a_ops = &lab5fs_aops;
	// }
	in_info = kmalloc(sizeof(struct lab5fs_inode_info), GFP_KERNEL);
	if (!in_info)
	{
		printk("lab5fs_read_inode: couldn't allocate struct lab5fs_inode_info\n");
		goto done;
	}
	in_info->i_sblock_dentries = INODE_TABLE_BLOCK_NO + 1;
	in_info->i_sblock_data = INODE_TABLE_BLOCK_NO + 1;
	in_info->i_eblock_data = in_info->i_sblock_data + 1;

	inode->u.generic_ip = in_info;
	inode->i_uid = di->i_uid;
	inode->i_gid = di->i_gid;
	inode->i_size = di->i_size;
	inode->i_blocks = di->i_blocks;
	inode->i_atime.tv_sec = di->i_atime;
	inode->i_mtime.tv_sec = di->i_mtime;
	inode->i_ctime.tv_sec = di->i_ctime;
	inode->i_atime.tv_nsec = di->i_atime;
	inode->i_mtime.tv_nsec = di->i_mtime;
	inode->i_ctime.tv_nsec = di->i_ctime;
	inode->i_op = &lab5fs_file_inops;
	inode->i_fop = &lab5fs_dir_operations;
	inode->i_mapping->a_ops = &lab5fs_aops;

	// if (!inode->i_ino)
	// inode->i_mode |= S_IFDIR;
done:
	brelse(bh);
	printk("lab5fs_read_inode (debug): done reading inode\n");
}
// int lab5fs_write_inode(struct inode *inode, int wait)
// {
// 	return 0;
// }
// void lab5fs_delete_inode(struct inode *inode)
// {
// }

static void lab5fs_clear_inode(struct inode *inode)
{
	printk("Inside lab5fs_clear_inode\n");
}
static void lab5fs_put_super(struct super_block *sb)
{
	printk("Inside lab5fs_put_super\n");
}
// static void lab5fs_write_super(struct super_block *sb)
// {
// }
struct super_operations lab5fs_sops = {
	.read_inode = lab5fs_read_inode,
	.clear_inode = lab5fs_clear_inode,
	.put_super = lab5fs_put_super,
};

static int lab5fs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head *bh, *bh_bitmap = NULL;
	struct lab5fs_super_block *lb5_sb;
	struct inode *root_inode;
	struct lab5fs_sb_info *sb_info;

	/* Initialize lab5fs private info */
	sb_info = kmalloc(sizeof(struct lab5fs_sb_info), GFP_KERNEL);
	printk("lab5fs (debug): inside lab5fs_fill_super.\n");
	if (!sb_info)
	{
		printk("lab5fs (debug): couldn't allocate memory for sb_info struct.\n");
		return -ENOMEM;
	}

	sb->s_fs_info = sb_info;
	memset(sb_info, 0, sizeof(*sb_info));
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
	if (!(bh_bitmap = sb_bread(sb, INODE_BITMAP_BLOCK_NO)))
	{
		printk(KERN_ERR "lab5fs: couldn't read inode bitmap from disk.\n");
		goto failed_sbi;
	}

	sb_info->inode_bitmap_bh = bh_bitmap;

	sb->s_magic = LAB5FS_MAGIC;
	sb->s_op = &lab5fs_sops;
	sb->s_blocksize = LAB5FS_BSIZE;
	sb->s_blocksize_bits = LAB5FS_BSIZE_BITS;
	sb->s_maxbytes = (((unsigned long long)1) << 32) - 1;
	root_inode = iget(sb, LAB5FS_ROOT_INODE);
	if (!root_inode)
	{
		printk("lab5fs (debug): failed iget.\n");
		goto failed_mount;
	}
	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root)
	{
		printk("lab5fs (debug): failed d_alloc_root.\n");
		iput(root_inode);
		goto failed_mount;
	}
	printk("lab5fs (debug): leaving lab5fs_fill_super.\n");

	return 0;

/* Handlers for fails */
failed_mount:
	brelse(bh);
	brelse(bh_bitmap);
	printk("lab5fs (debug): inside failed_mount.\n");

failed_sbi:
	printk("lab5fs (debug): inside failed_sbi.\n");
	sb->s_fs_info = NULL;
	kfree(sb_info);
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

/** TODO: Revisit operation structs to add/remove functionalites as needed */
static struct inode_operations lab5fs_file_inops = {
	.create = lab5fs_create,
	.link = lab5fs_link,
	.unlink = lab5fs_unlink,
	.lookup = lab5fs_lookup,
};
static struct file_operations lab5fs_file_operations = {
	.llseek = generic_file_llseek,
	.read = generic_file_read,
	.write = generic_file_write,
	.mmap = generic_file_mmap,
	.open = generic_file_open,
};
struct file_operations lab5fs_dir_operations = {
	.read = generic_read_dir,
	.readdir = lab5fs_readdir,
	.fsync = file_fsync,
};
