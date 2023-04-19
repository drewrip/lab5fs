#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/vfs.h>
#include <linux/smp_lock.h>
#include "lab5fs.h"

/* Declare static functions for file system operations */
static int lab5fs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd);
static int lab5fs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry);
static int lab5fs_unlink(struct inode *dir, struct dentry *dentry);
static struct dentry *lab5fs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd);
static int lab5fs_readdir(struct file *flip, void *dirent, filldir_t filldir);
static struct buffer_head *lab5fs_find_entry(struct inode *dir, const char *name, int namelen, struct lab5fs_dir_entry **res_dir);
static int lab5fs_readpage(struct file *file, struct page *page);
static int lab5fs_writepage(struct page *page, struct writeback_control *wbc);
static int lab5fs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to);
static sector_t lab5fs_bmap(struct address_space *mapping, sector_t block);

MODULE_LICENSE("GPL");
static int lab5fs_get_block(struct inode *inode, sector_t block,
							struct buffer_head *bh_result, int create)
{
	printk("lab5fs_get_block (debug): inside lab5fs_get_block\n");
	return 0;
}
static int lab5fs_readpage(struct file *file, struct page *page)
{
	printk("lab5fs_readpage (debug): inside lab5fs_readpage\n");
	return block_read_full_page(page, lab5fs_get_block);
}
static int lab5fs_writepage(struct page *page, struct writeback_control *wbc)
{
	printk("lab5fs_writepage (debug): inside lab5fs_writepage\n");
	return block_write_full_page(page, lab5fs_get_block, wbc);
}
static int lab5fs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	printk("lab5fs_prepare_write (debug): inside lab5fs_prepare_write\n");
	return block_prepare_write(page, from, to, lab5fs_get_block);
}
static sector_t lab5fs_bmap(struct address_space *mapping, sector_t block)
{
	printk("lab5fs_bmap (debug): inside lab5fs_bmap\n");
	return generic_block_bmap(mapping, block, lab5fs_get_block);
}

static int lab5fs_add_entry(struct dentry *dentry, struct inode *inode)
{
	printk("lab5fs_add_entry (debug): inside lab5fs_add_entry\n");
	struct buffer_head *bh_dir;
	struct lab5fs_inode_info *in_info;
	struct lab5fs_dir_entry *current_lab5fs_de;
	int offset, block_no = 0, data_sblock_no, data_eblock_no;
	struct inode *parent_inode = dentry->d_parent->d_inode;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	unsigned short current_de_rec_len = 0, current_de_aligned_rec_len,
				   /* Get the rec_len of the future entry 4 bytes aligned */
		future_de_rec_len = REC_LEN_ALIGN_FOUR(namelen);
	in_info = inode->u.generic_ip;
	data_sblock_no = in_info->i_sblock_data;
	data_eblock_no = in_info->i_eblock_data;
	printk("lab5fs_add_entry: sblock %lu and eblock %lu\n", data_sblock_no, data_eblock_no);
	printk("lab5fs_add_entry (debug):passed assignment stage\n");
	if (!namelen)
	{
		printk("lab5fs_add_entry: entry doesn't exist\n");
		return -ENOENT;
	}
	if (namelen > MAX_FILE_NAME_LENGTH)
	{
		printk("lab5fs_add_entry: entry name exceed max name length\n");
		return -ENAMETOOLONG;
	}
	printk("lab5fs_add_entry (debug): before for loop stage\n");
	/* Iterate through data blocks until we find a free chunk */
	for (block_no = data_sblock_no; block_no <= data_eblock_no; block_no++)
	{
		/* Read the block from disk */
		bh_dir = sb_bread(inode->i_sb, block_no);
		if (!bh_dir)
		{
			printk("lab5fs_add_entry: couldn't find space for dentry in block %lu\n", block_no);
			return -ENOSPC;
		}

		/* If we get here, that means that there might be space, so we look for a free chunk within the block */
		offset = 0;
		for (; offset < LAB5FS_BSIZE; offset += current_de_rec_len)
		{
			/* Let's obtain thet lab5fs dentry from disk to check if it's empty */
			current_lab5fs_de = (struct lab5fs_dir_entry *)(bh_dir->b_data + offset);
			/* If inode number isn't set and the chunk's size can accomodate out entry, we presist the data */

			if (!current_lab5fs_de->inode)
				goto found_chunk;
			current_de_rec_len += current_lab5fs_de->rec_len;
			printk("lab5fs_add_entry (debug): skipped over inode %lu with name %s and offset %lu\n", current_lab5fs_de->inode, current_lab5fs_de->name, current_de_rec_len);
			/*  If the last statement didn't pass, don't panic, we can still find space.
			 *  Let's see if at least the curent entry has some padding that can fit our entry.
			 */
			current_de_aligned_rec_len = REC_LEN_ALIGN_FOUR(current_lab5fs_de->namelen);
			// if (current_de_rec_len >= future_de_rec_len + current_de_aligned_rec_len)
			// 	goto found_chunk;
		}
		brelse(bh_dir);
	}
	/* Uh no, there's no space */
	printk("leaving lab5fs_add_entry after finding no space\n");
	return -ENOSPC;

found_chunk:
	printk("lab5fs_add_entry: inside found_chunk with rec_len %hu, namelen %hu, name %s, inode %lu, and offset %lu\n", current_lab5fs_de->rec_len, current_lab5fs_de->namelen, current_lab5fs_de->name, current_lab5fs_de->inode, current_de_rec_len);
	current_lab5fs_de->rec_len = REC_LEN_ALIGN_FOUR(namelen);
	/* Handle case where we're adding the dentry into the chunk of another dentry */
	if (current_lab5fs_de->inode)
	{
		printk("lab5fs_add_entry (debug): inside if block in found_chunk stage\n");
		struct lab5fs_dir_entry *tmp_de = (struct lab5fs_dir_entry *)((char *)current_lab5fs_de + current_de_aligned_rec_len);
		tmp_de->rec_len = current_lab5fs_de->rec_len - current_de_aligned_rec_len;
		current_lab5fs_de->rec_len = current_de_aligned_rec_len;
		current_lab5fs_de = tmp_de;
	}
	printk("lab5fs_add_entry (debug): after found_chunk stage\n");
	current_lab5fs_de->inode = inode->i_ino;
	current_lab5fs_de->namelen = namelen;
	current_lab5fs_de->file_type = 2;
	memcpy(current_lab5fs_de->name, name, namelen);

	/* Update parent inode */

	parent_inode->i_mtime = parent_inode->i_ctime = CURRENT_TIME;
	printk("lab5fs_add_entry (debug): after parent assignment stage\n");
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
	// lock_kernel();
	in_info = kmalloc(sizeof(struct lab5fs_inode_info), GFP_KERNEL);
	if (!in_info)
	{
		// unlock_kernel();
		iput(inode);
		printk("lab5fs_create: couldn't find space to allocate lab5fs_sb_info\n");
		return -ENOSPC;
	}
	/* Update file offset in the event that the block read failed */
	if (!bh_bitmap)
	{
		// unlock_kernel();
		printk("lab5fs_create: couldn't read inode bitmap\n");
		return -1;
	}
	printk("lab5fs_create (debug): passed bitmap block stage\n");
	ino = find_first_zero_bit(b_map->bitmap, LAB5FS_BSIZE);
	printk("lab5fs_create (debug): passed bitmap block bit set\n");
	if (ino > LAB5FS_BSIZE)
	{
		// unlock_kernel();
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
	inode->i_sb = sb;
	inode->i_blksize = LAB5FS_BSIZE;
	inode->i_blkbits = LAB5FS_BSIZE_BITS;

	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_op = &lab5fs_file_inops;
	if (S_ISREG(inode->i_mode))
	{
		inode->i_fop = &lab5fs_file_operations;
		inode->i_mapping->a_ops = &lab5fs_aops;
		inode->i_mode |= S_IFREG;
	}
	else if (S_ISDIR(inode->i_mode))
	{
		inode->i_fop = &lab5fs_dir_operations;
		inode->i_mode |= S_IFDIR;
	}
	inode->i_nlink = 1;
	inode->u.generic_ip = in_info;
	in_info->i_sblock_data = sb_info->s_first_data_block;
	in_info->i_eblock_data = sb_info->s_blocks_count - 1;

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
		unlock_kernel();
		return res;
	}

	// unlock_kernel();
	d_instantiate(dentry, inode);
	printk("lab5fs_create (debug): passed inode assignment stage 2\n");
	printk("lab5fs_create (debug): leaving lab5fs_create\n");
	return 0;
}
static int lab5fs_unlink(struct inode *dir, struct dentry *dentry)
{
	/** TODO: Complete function*/
	int error = -ENOENT;
	struct inode *inode;
	struct buffer_head *bh;
	struct lab5fs_dir_entry *de;

	printk("Inside lab5fs_unlink\n");
	inode = dentry->d_inode;
	lock_kernel();

	bh = lab5fs_find_entry(dir, dentry->d_name.name, dentry->d_name.len, &de);
	if (!bh || de->inode != inode->i_ino)
		goto out_brelse;

	if (!inode->i_nlink)
	{
		printk("unlinking non-existent file %s\n", inode->i_sb->s_id);
		inode->i_nlink = 1;
	}
	de->inode = 0;
	mark_buffer_dirty(bh);
	dir->i_ctime = dir->i_mtime = CURRENT_TIME;
	mark_inode_dirty(dir);
	inode->i_nlink--;
	inode->i_ctime = dir->i_ctime;
	mark_inode_dirty(inode);
	error = 0;
out_brelse:
	brelse(bh);
	unlock_kernel();
	return error;
}

static int lab5fs_link(struct dentry *old_dentry, struct inode *dir,
					   struct dentry *dentry)
{
	struct inode *inode = old_dentry->d_inode;
	int err;

	lock_kernel();
	/* BFS does some locking here via lock_kernel(), gonna omit for now*/
	err = lab5fs_add_entry(dentry, dir);
	if (err)
	{
		printk("ERROR linking\n");
		return err;
	}
	inode->i_nlink++;
	inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
	atomic_inc(&inode->i_count);
	d_instantiate(dentry, inode);
	unlock_kernel();
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

		printk("lab5fs_find_entry: checking %s\n");
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
	lock_kernel();
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
			unlock_kernel();
			return ERR_PTR(-EACCES);
		}
	}
	/* If we get here, the dentry is valid and confirm, so we add to the dcache */
	printk("lab5fs_lookup: calling d_add on inode %p, name %s, and length %d\n", inode_of_dir, dentry->d_name.name, dentry->d_name.len);
	unlock_kernel();
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

	lock_kernel();

	// /* Ensure that we don't read past the boundary of a dir entry */
	// if (flip->f_pos & (sizeof(struct lab5fs_dir_entry) - 1))
	// {
	// 	printk("lab5fs_readdir: attempted to read beyond file where result is %llu\n", flip->f_pos & (sizeof(struct lab5fs_dir_entry) - 1));
	// 	unlock_kernel();
	// 	return -1;
	// }
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
	printk("flip->f_pos is %lld d_ino->i_size is %lu and inode number is %lu\n", flip->f_pos, d_ino->i_size, d_ino->i_ino);
	while (flip->f_pos < d_ino->i_size)
	{
		/* Calculate offset within a lab5fs block */
		b_offset = flip->f_pos & (LAB5FS_BSIZE - 1);

		/* Obtain starting block number for dentries */
		block_no = in_info->i_sblock_data + (flip->f_pos >> LAB5FS_BSIZE_BITS);

		printk("block no is %lu and offset is %lu\n", block_no, b_offset);
		/* Let's read the block and begin dir reading */
		bh = sb_bread(d_ino->i_sb, block_no);
		/* Update file offset in the event that the block read failed */
		if (!bh)
		{
			flip->f_pos += LAB5FS_BSIZE - b_offset;
			printk("skipping buffer\n");
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
			if (lab5fs_dentry->inode)
			{
				// printk("lab5fs_readdir: calling filldir on name %s and inode %lu\n", lab5fs_dentry->name, lab5fs_dentry->inode);
				if (filldir(dirent, lab5fs_dentry->name, lab5fs_dentry->namelen, flip->f_pos, lab5fs_dentry->inode, DT_UNKNOWN) < 0)
				{
					brelse(bh);
					unlock_kernel();
					return 0;
				}
				b_offset += lab5fs_dentry->rec_len;
				flip->f_pos += lab5fs_dentry->rec_len;
				printk("lab5fs_readdir: done calling filldir and for inode %lu, name %s, namelen is %hu, and rec_len %hu\n", lab5fs_dentry->inode, lab5fs_dentry->name, lab5fs_dentry->namelen, lab5fs_dentry->rec_len);
			}
			else
			{
				printk("lab5fs_readdir: skipped filldir with inode %lu and name %s\n", lab5fs_dentry->inode, lab5fs_dentry->name);
				b_offset += sizeof(lab5fs_dentry);
				flip->f_pos += sizeof(lab5fs_dentry);
			}

		} while (b_offset < LAB5FS_BSIZE && flip->f_pos < d_ino->i_size);
		brelse(bh);
	}
	printk("Existing lab5fs_readdir\n");
	unlock_kernel();
	return 0;
}

/** TODO: Clean up function and add/remove logic as needed */
void lab5fs_read_inode(struct inode *inode)
{
	printk("lab5fs_read_inode (debug): reading inode\n");
	unsigned long ino = inode->i_ino;
	struct lab5fs_inode *di;
	struct buffer_head *bh;
	struct lab5fs_inode_info *in_info;
	struct lab5fs_sb_info *sb_info = (struct lab5fs_sb_info *)inode->i_sb->s_fs_info;
	int offset, block_no;

	if (!inode)
	{
		printk("lab5fs_read_inode: attempted to read NULL inode\n");
		return;
	}
	printk("lab5fs_read_inode: ino is %lu\n", ino);
	block_no = (ino - LAB5FS_ROOT_INODE) / (LAB5FS_BSIZE / sizeof(struct lab5fs_inode));
	/* Get corrersponding block number to the inode number so that we read from the right place */
	bh = sb_bread(inode->i_sb, block_no + INODE_TABLE_BLOCK_NO);
	if (!bh)
	{
		printk(KERN_ERR "lab5fs_read_inode: sb_bread returned null for block sector\n");
		return;
	}

	offset = (ino - LAB5FS_ROOT_INODE) % (LAB5FS_BSIZE / sizeof(struct lab5fs_inode));
	di = (struct lab5fs_inode *)bh->b_data + offset;

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
	in_info->i_sblock_data = sb_info->s_first_data_block;
	in_info->i_eblock_data = sb_info->s_blocks_count - in_info->i_sblock_data - 1;

	inode->u.generic_ip = in_info;
	inode->i_uid = di->i_uid;
	inode->i_gid = di->i_gid;
	inode->i_size = ino == LAB5FS_ROOT_INODE ? LAB5FS_BSIZE : di->i_size;
	inode->i_nlink = ino == LAB5FS_ROOT_INODE ? 2 : di->i_links_count;
	inode->i_blksize = LAB5FS_BSIZE;
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

static void lab5fs_delete_inode(struct inode *inode)
{
	printk("Inside lab5fs_clear_inode\n");
}
static void lab5fs_put_super(struct super_block *sb)
{
	printk("Inside lab5fs_put_super\n");
	struct lab5fs_sb_info *sb_info = sb->s_fs_info;
	/* Release resources */
	brelse(sb_info->inode_bitmap_bh);
	kfree(sb_info);
	sb->s_fs_info = NULL;
}
// static void lab5fs_write_super(struct super_block *sb)
// {
// }
int lab5fs_write_inode(struct inode *inode, int unused)

{
	printk("Inside lab5fs_write_inode\n");
	unsigned long ino = inode->i_ino;
	struct lab5fs_inode *di;
	struct buffer_head *bh;
	int block, off;

	if (ino < LAB5FS_ROOT_INODE)
	{
		printk("bad inode number\n");
		return -EIO;
	}
	lock_kernel();
	block = (ino - LAB5FS_ROOT_INODE) / LAB5FS_INODES_PER_BLOCK + 1;
	bh = sb_bread(inode->i_sb, block);
	if (!bh)
	{
		printk("unable to read inode\n");
		unlock_kernel();
		return -EIO;
	}

	off = (ino - LAB5FS_ROOT_INODE) % LAB5FS_INODES_PER_BLOCK;
	di = (struct lab5fs_inode *)bh->b_data + off;

	//	if(inode->i_ino == LAB5FS_ROOT_INODE)
	//		di->i_vtype = LAB5FS_VDIR;
	//	else
	//		di->i_vtype = LAB5FS_VREG;

	di->i_ino = inode->i_ino;
	di->i_mode = inode->i_mode;
	di->i_uid = inode->i_uid;
	di->i_gid = inode->i_gid;
	di->i_links_count = inode->i_nlink;
	di->i_atime = inode->i_atime.tv_sec;
	di->i_mtime = inode->i_mtime.tv_sec;
	di->i_ctime = inode->i_ctime.tv_sec;
	// di->i_sblock_data = LAB5FS_I(inode)->i_sblock_data;
	// di->i_eblock_data = LAB5FS_I(inode)->i_eblock_data;
	// di->i_eoffset = di->i_sblock_data * LAB5FS_BSIZE + inode->i_size - 1;

	mark_buffer_dirty(bh);
	brelse(bh);
	unlock_kernel();
	return 0;
}
struct super_operations lab5fs_sops = {
	.read_inode = lab5fs_read_inode,
	.write_inode = lab5fs_write_inode,
	.delete_inode = lab5fs_delete_inode,
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
	sb_info->s_first_data_block = lb5_sb->s_first_data_block;
	sb_info->s_blocks_count = lb5_sb->s_blocks_count;

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
	.fsync = file_fsync,
};
struct file_operations lab5fs_dir_operations = {
	.read = generic_read_dir,
	.readdir = lab5fs_readdir,
	.fsync = file_fsync,
	.open = dcache_dir_open,
	.release = dcache_dir_close,
	.llseek = dcache_dir_lseek,
	.read = generic_read_dir,
};
static struct address_space_operations lab5fs_aops = {
	.writepage = lab5fs_writepage,
	.prepare_write = lab5fs_prepare_write,
	.commit_write = generic_commit_write,
	.sync_page = block_sync_page,
	.readpage = lab5fs_readpage,
	.bmap = lab5fs_bmap,
};