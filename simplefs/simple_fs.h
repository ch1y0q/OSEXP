/**
 * adopted from include/uapi/linux/minix_fs.h
*/

#ifndef _SIMPLE_FS_H
#define _SIMPLE_FS_H


#include <linux/types.h>
#include <linux/magic.h>
#include <linux/fs.h>

/*
 * The simple filesystem constants/structures
 */

#define SIMPLE_ROOT_INO 1
#define SIMPLE_VERSION 1
/* Not the same as the bogus LINK_MAX in <linux/limits.h>. Oh well. */
#define SIMPLE_LINK_MAX	65530

#define SIMPLE_I_MAP_SLOTS	8
#define SIMPLE_Z_MAP_SLOTS	64
#define SIMPLE_VALID_FS		0x0001		/* Clean fs. */
#define SIMPLE_ERROR_FS		0x0002		/* fs has errors. */

#define SIMPLE_BLOCK_SIZE_BITS 10
#define SIMPLE_BLOCK_SIZE     (1 << SIMPLE_BLOCK_SIZE_BITS)

#define SIMPLE_INODES_PER_BLOCK ((SIMPLE_BLOCK_SIZE)/(sizeof (struct simple_inode)))

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

#define SIMPLE_MAGIC   0x6e616976	/* naiv */

/* ================= */
/*  for mkfs.simple  */
/* ================= */

/*
 * Global variables.
 */
extern char *super_block_buffer;

#define Super (*(struct simple_super_block *) super_block_buffer)

#define INODE_SIZE (sizeof(struct simple_inode))
#define INODE2_SIZE (sizeof(struct simple_inode))

#define BITS_PER_BLOCK (SIMPLE_BLOCK_SIZE << 3)

#define UPPER(size,n) ((size+((n)-1))/(n))

/*
 * Inline functions.
 */
static inline unsigned long get_ninodes(void)
{
	return Super.s_ninodes;
}

static inline unsigned long get_nzones(void)
{
	return Super.s_zones;
}

static inline unsigned long get_nimaps(void)
{
	return Super.s_imap_blocks;
}

static inline unsigned long get_nzmaps(void)
{
	return Super.s_zmap_blocks;
}

static inline off_t get_first_zone(void)
{
	return Super.s_firstdatazone;
}

static inline size_t get_zone_size(void)
{
	return Super.s_log_zone_size;
}

static inline size_t get_max_size(void)
{
	return Super.s_max_size;
}

static inline unsigned long inode_blocks(void)
{
	return UPPER(get_ninodes(), SIMPLE_INODES_PER_BLOCK);
}

static inline off_t first_zone_data(void)
{
	return 2 + get_nimaps() + get_nzmaps() + inode_blocks();
}

static inline size_t get_inode_buffer_size(void)
{
	return inode_blocks() * SIMPLE_BLOCK_SIZE;
}

/*
 * simple fs inode data in memory
 */
struct simple_inode_info {
	union {
		__u16 i1_data[16];
		__u32 i2_data[16];
	} u;
	struct inode vfs_inode;
};

/*
 * simple super-block data in memory
 */
struct simple_sb_info {
	unsigned long s_ninodes;
	unsigned long s_nzones;
	unsigned long s_imap_blocks;
	unsigned long s_zmap_blocks;
	unsigned long s_firstdatazone;
	unsigned long s_log_zone_size;
	int s_dirsize;
	int s_namelen;
	struct buffer_head ** s_imap;
	struct buffer_head ** s_zmap;
	struct buffer_head * s_sbh;
	struct simple_super_block * s_ms;
	unsigned short s_mount_state;
	unsigned short s_version;
};


/*
 * little-endian bitmaps
 */

#define simple_test_and_set_bit	__test_and_set_bit_le
#define simple_set_bit		__set_bit_le
#define simple_test_and_clear_bit	__test_and_clear_bit_le
#define simple_test_bit	test_bit_le
#define simple_find_first_zero_bit	find_first_zero_bit_le

#endif