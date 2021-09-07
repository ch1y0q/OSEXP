/**
 * adopted from include/uapi/linux/minix_fs.h
*/

#ifndef _SIMPLE_FS_H
#define _SIMPLE_FS_H

#include <linux/types.h>
#include <linux/magic.h>

/*
 * The simple filesystem constants/structures
 */

#define SIMPLE_ROOT_INO 1

/* Not the same as the bogus LINK_MAX in <linux/limits.h>. Oh well. */
#define SIMPLE_LINK_MAX	65530

#define SIMPLE_I_MAP_SLOTS	8
#define SIMPLE_Z_MAP_SLOTS	64
#define SIMPLE_VALID_FS		0x0001		/* Clean fs. */
#define SIMPLE_ERROR_FS		0x0002		/* fs has errors. */

#define SIMPLE_INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct simple_inode)))

/*
 * The new minix inode has all the time entries, as well as
 * long block numbers and a third indirect block (7+1+1+1
 * instead of 7+1+1). Also, some previously 8-bit values are
 * now 16-bit. The inode is now 64 bytes instead of 32.
 */
struct simple_inode {
	__u16 i_mode;
	__u16 i_nlinks;
	__u16 i_uid;
	__u16 i_gid;
	__u32 i_size;
	__u32 i_atime;
	__u32 i_mtime;
	__u32 i_ctime;
	__u32 i_zone[10];
};

/*
 * simple super-block data on disk
 */
struct simple_super_block {
	__u16 s_ninodes;
	__u16 s_nzones;
	__u16 s_imap_blocks;
	__u16 s_zmap_blocks;
	__u16 s_firstdatazone;
	__u16 s_log_zone_size;
	__u32 s_max_size;
	__u16 s_magic;
	__u16 s_state;
	__u32 s_zones;
};

struct simple_dir_entry {
	__u16 inode;
	char name[0];   /* malloc(sizeof(struct simple_dir_entry)+...) */
};

#endif