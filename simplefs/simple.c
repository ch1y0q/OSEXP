/**
 * Implementation of module simplefs
*/

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/highuid.h>
#include <linux/vfs.h>
#include <linux/writeback.h>

#include "simple_fs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HUANG Qiyue");
MODULE_DESCRIPTION("A Simple Simple filesystem module");


static struct inode *simple_iget(struct super_block *sb,  unsigned long ino);



/* ================= */
/*       bitmap      */
/* ================= */

struct simple_inode *
simple_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)
{
	int block;
	struct simple_sb_info *sbi = sb->s_fs_info;
	struct simple_inode *p;
	int inodes_per_block = sb->s_blocksize / sizeof(struct simple_inode);

	*bh = NULL;
	if (!ino || ino > sbi->s_ninodes) {
		printk("Bad inode number on dev %s: %ld is out of range\n",
		       sb->s_id, (long)ino);
		return NULL;
	}
	ino--;
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks +
		 ino / inodes_per_block;
	*bh = sb_bread(sb, block);
	if (!*bh) {
		printk("Unable to read inode block\n");
		return NULL;
	}
	p = (void *)(*bh)->b_data;
	return p + ino % inodes_per_block;
}



/* ================= */
/*    super-block    */
/* ================= */


static void simple_put_super(struct super_block *sb)
{
	int i;
	struct simple_sb_info *sbi = sb->s_fs_info;

	if (!sb_rdonly(sb)) {
		sbi->s_ms->s_state = sbi->s_mount_state;
			mark_buffer_dirty(sbi->s_sbh);
	}
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	brelse (sbi->s_sbh);
	kfree(sbi->s_imap);
	sb->s_fs_info = NULL;
	kfree(sbi);
}

static struct super_operations simple_sops = {
    .alloc_inode = NULL,//simple_alloc_inode,
    .destroy_inode = NULL,//simple_destroy_inode,
    //.read_inode is obsolete, 
    // see commit 12debc4248a4a7f1873e47cda2cdd7faca80b099
    .put_super = simple_put_super,
};

static int simplefs_fill_super(struct super_block *s, void *data, int silent)
{
	struct buffer_head *bh;
	struct buffer_head **map;
	struct simple_super_block *ms;
	unsigned long i, block;
	struct inode *root_inode;
	struct simple_sb_info *sbi;
	int ret = -EINVAL;

	sbi = kzalloc(sizeof(struct simple_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	s->s_fs_info = sbi;

	if (!sb_set_blocksize(s, BLOCK_SIZE))
		goto out_bad_hblock;

	if (!(bh = sb_bread(s, 1)))
		goto out_bad_sb;

	ms = (struct simple_super_block *) bh->b_data;
	sbi->s_ms = ms;
	sbi->s_sbh = bh;
	sbi->s_mount_state = ms->s_state;
	sbi->s_ninodes = ms->s_ninodes;
	sbi->s_nzones = ms->s_nzones;
	sbi->s_imap_blocks = ms->s_imap_blocks;
	sbi->s_zmap_blocks = ms->s_zmap_blocks;
	sbi->s_firstdatazone = ms->s_firstdatazone;
	sbi->s_log_zone_size = ms->s_log_zone_size;
	s->s_maxbytes = ms->s_max_size;
	s->s_magic = ms->s_magic;
	if (s->s_magic == SIMPLE_MAGIC) {
		sbi->s_version = SIMPLE_VERSION;
		sbi->s_nzones = ms->s_zones;
		sbi->s_dirsize = 32;
		sbi->s_namelen = 30;
		s->s_max_links = SIMPLE_LINK_MAX;
	}
	else
		goto out_no_fs;

	/*
	 * Allocate the buffer map to keep the superblock small.
	 */
	i = (sbi->s_imap_blocks + sbi->s_zmap_blocks) * sizeof(bh);
	map = kzalloc(i, GFP_KERNEL);
	if (!map)
		goto out_no_map;
	sbi->s_imap = &map[0];
	sbi->s_zmap = &map[sbi->s_imap_blocks];

	block=2;
	for (i=0 ; i < sbi->s_imap_blocks ; i++) {
		if (!(sbi->s_imap[i]=sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}
	for (i=0 ; i < sbi->s_zmap_blocks ; i++) {
		if (!(sbi->s_zmap[i]=sb_bread(s, block)))
			goto out_no_bitmap;
		block++;
	}

	simple_set_bit(0,sbi->s_imap[0]->b_data);
	simple_set_bit(0,sbi->s_zmap[0]->b_data);

	/* set up enough so that it can read an inode */
	s->s_op = &simple_sops;
	s->s_time_min = 0;
	s->s_time_max = U32_MAX;
	root_inode = simple_iget(s, SIMPLE_ROOT_INO);
	if (IS_ERR(root_inode)) {
		ret = PTR_ERR(root_inode);
		goto out_no_root;
	}

	ret = -ENOMEM;
	s->s_root = d_make_root(root_inode);
	if (!s->s_root)
		goto out_no_root;

	if (!sb_rdonly(s)) {
		ms->s_state &= ~SIMPLE_VALID_FS;
		mark_buffer_dirty(bh);
	}
	if (!(sbi->s_mount_state & SIMPLE_VALID_FS))
		printk("SIMPLE-fs: mounting unchecked file system, "
			"running fsck is recommended\n");
	else if (sbi->s_mount_state & SIMPLE_ERROR_FS)
		printk("SIMPLE-fs: mounting file system with errors, "
			"running fsck is recommended\n");

	return 0;

/* exit on errors */

out_no_root:
	if (!silent)
		printk("SIMPLE-fs: get root inode failed\n");
	goto out_freemap;

out_no_bitmap:
	printk("SIMPLE-fs: bad superblock or unable to read bitmaps\n");

out_freemap:
	for (i = 0; i < sbi->s_imap_blocks; i++)
		brelse(sbi->s_imap[i]);
	for (i = 0; i < sbi->s_zmap_blocks; i++)
		brelse(sbi->s_zmap[i]);
	kfree(sbi->s_imap);
	goto out_release;

out_no_map:
	ret = -ENOMEM;
	if (!silent)
		printk("SIMPLE-fs: can't allocate map\n");
	goto out_release;

out_no_fs:
	if (!silent)
		printk("VFS: Can't find a simple filesystem "
		       "on device %s.\n", s->s_id);
out_release:
	brelse(bh);
	goto out;

out_bad_hblock:
	printk("SIMPLE-fs: blocksize too small for device\n");
	goto out;

out_bad_sb:
	printk("SIMPLE-fs: unable to read superblock\n");
out:
	s->s_fs_info = NULL;
	kfree(sbi);
	return ret;
}

/* ================= */
/*       file        */
/* ================= */
const struct file_operations simple_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
};

int simple_getattr(struct user_namespace *mnt_userns, const struct path *path,
		  struct kstat *stat, u32 request_mask, unsigned int flags)
{
	struct super_block *sb = path->dentry->d_sb;
	struct inode *inode = d_inode(path->dentry);

	generic_fillattr(&init_user_ns, inode, stat);
	stat->blocks = (sb->s_blocksize / 512) * nblocks(stat->size, sb);
	stat->blksize = sb->s_blocksize;
	return 0;
}

static int simplefs_setattr(struct user_namespace *mnt_userns,
			 struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	int error;

	error = setattr_prepare(&init_user_ns, dentry, attr);
	if (error)
		return error;

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size != i_size_read(inode)) {
		error = inode_newsize_ok(inode, attr->ia_size);
		if (error)
			return error;

		truncate_setsize(inode, attr->ia_size);
		simple_truncate(inode);
	}

	setattr_copy(&init_user_ns, inode, attr);
	mark_inode_dirty(inode);
	return 0;
}

const struct inode_operations simple_file_inode_operations = {
	.setattr	= simplefs_setattr,
	.getattr	= NULL,//TODO: simple_getattr,
};

/* ================= */
/*        dir        */
/* ================= */

const struct file_operations simple_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.fsync		= generic_file_fsync,
};

/* ================= */
/*       inode       */
/* ================= */

void simple_truncate(struct inode *inode){
        if(!(S_ISREG(inode->i_mode)||S_ISDIR(inode->i_mode)||S_ISLNK(inode->i_mode)))
                return;
        struct simple_inode_info *si = inode->s_fs_info;
        block_truncate_page(inode->i_mapping,inode->i_size,simple_get_block);

        si->i_reserved+=si->i_end_block-si->i_start_block+1;//reserved block
        si->i_end_block=si->i_start_block;//clear
        inode->i_mtime=inode->i_ctime=current_time(inode);
        mark_inode_dirty(inode);
}

static struct inode *simple_iget(struct super_block *sb,  unsigned long ino)
{
	struct inode *inode;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	struct buffer_head * bh;
	struct simple_inode * raw_inode;
	struct simple_inode_info *simple_inode = container_of(inode, struct simple_inode_info, vfs_inode);
	int i;

	raw_inode = simple_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode) {
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	if (raw_inode->i_nlinks == 0) {
		printk("simplefs: deleted inode referenced: %lu\n",
		       inode->i_ino);
		brelse(bh);
		iget_failed(inode);
		return ERR_PTR(-ESTALE);
	}
	inode->i_mode = raw_inode->i_mode;
	i_uid_write(inode, raw_inode->i_uid);
	i_gid_write(inode, raw_inode->i_gid);
	set_nlink(inode, raw_inode->i_nlinks);
	inode->i_size = raw_inode->i_size;
	inode->i_mtime.tv_sec = raw_inode->i_mtime;
	inode->i_atime.tv_sec = raw_inode->i_atime;
	inode->i_ctime.tv_sec = raw_inode->i_ctime;
	inode->i_mtime.tv_nsec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_ctime.tv_nsec = 0;
	inode->i_blocks = 0;
	for (i = 0; i < 10; i++)
		simple_inode->u.i2_data[i] = raw_inode->i_zone[i];
	simple_set_inode(inode, old_decode_dev(raw_inode->i_zone[0]));
	brelse(bh);
	unlock_new_inode(inode);
	return inode;
}

/* register ops for regular file, dir and link */
void simple_set_inode(struct inode *inode, dev_t rdev)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &simple_file_inode_operations;
		inode->i_fop = &simple_file_operations;
		//TODO inode->i_mapping->a_ops = &simple_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		//TODO inode->i_op = &simple_dir_inode_operations;
		inode->i_fop = &simple_dir_operations;
		//TODO inode->i_mapping->a_ops = &simple_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		//TODO inode->i_op = &simple_symlink_inode_operations;
		inode_nohighmem(inode);
		//TODO inode->i_mapping->a_ops = &simple_aops;
	} else
		init_special_inode(inode, inode->i_mode, rdev);
}

static struct dentry *simple_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, simplefs_fill_super);
}

static struct file_system_type simple_fs_type = {
    .owner = THIS_MODULE,
    .name = "simple",
    .fs_flags = FS_REQUIRES_DEV,
    .mount = simple_mount,
    .kill_sb = kill_block_super,
};

MODULE_ALIAS_FS("simple");

static int __init init_simple_fs(void)
{
    int retval;
    retval = register_filesystem(&simple_fs_type);
    printk(KERN_INFO "Simplefs register returned: %d\n", retval);
    return retval;
}

static void __exit exit_simple_fs(void)
{
    unregister_filesystem(&simple_fs_type);
    printk(KERN_INFO "Simplefs unregistered.\n");
}

module_init(init_simple_fs)
module_exit(exit_simple_fs)