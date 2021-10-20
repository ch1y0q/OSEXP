/**
 * Utility to make a Simple Filesystem.
 * This is adapted from mkfs.minix.c, based on version 2.
*/

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/stat.h>
#include <getopt.h>
#include <err.h>

#include "blkdev.h"
#include "nls.h"
#include "pathnames.h"
#include "bitops.h"
#include "exitcodes.h"
#include "strutils.h"
#include "all-io.h"
#include "closestream.h"
#include "ismounted.h"

#include "simple_fs.h"

#define XALLOC_EXIT_CODE MKFS_EX_ERROR
#include "xalloc.h"

#define SIMPLE_ROOT_INO 1
#define SIMPLE_BAD_INO 2

#define TEST_BUFFER_BLOCKS 16
#define MAX_GOOD_BLOCKS 512

#define SIMPLE_MAX_INODES 65535

char *super_block_buffer;

static char *inode_buffer = NULL;

#define Inode (((struct simple_inode *) inode_buffer) - 1)

struct fs_control {
	char *device_name;		/* device on which fs is created */
	int device_fd;			/* open file descriptor of the device */
	char *lockmode;			/* as specified by --lock */
	unsigned long long fs_blocks;	/* device block count for the file system */
	int fs_used_blocks;		/* used blocks on a device */
	int fs_bad_blocks;		/* number of bad blocks found from device */
	uint16_t fs_namelen;		/* maximum length of filenames */
	size_t fs_dirsize;		/* maximum size of directory */
	unsigned long fs_inodes;	/* number of inodes */
	int fs_magic;			/* file system magic number */
	unsigned int
	 check_blocks:1;		/* check for bad blocks */
};

static char root_block[SIMPLE_BLOCK_SIZE];
static char boot_block_buffer[512];
static unsigned short good_blocks_table[MAX_GOOD_BLOCKS];

static char *inode_map;
static char *zone_map;

#define zone_in_use(x) (isset(zone_map,(x)-get_first_zone()+1) != 0)

#define mark_inode(x) (setbit(inode_map,(x)))
#define unmark_inode(x) (clrbit(inode_map,(x)))

#define mark_zone(x) (setbit(zone_map,(x)-get_first_zone()+1))
#define unmark_zone(x) (clrbit(zone_map,(x)-get_first_zone()+1))

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options] /dev/name [blocks]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -i, --inodes <num>      number of inodes for the filesystem\n"), out);
	fputs(_(" -c, --check             check the device for bad blocks\n"), out);
	fputs(_(" -l, --badblocks <file>  list of bad blocks from file\n"), out);
	fprintf(out, _(
		"     --lock[=<mode>]     use exclusive device lock (%s, %s or %s)\n"), "yes", "no", "nonblock");
	fputs(USAGE_SEPARATOR, out);
	printf(USAGE_HELP_OPTIONS(25));
	exit(MKFS_EX_OK);
}

# define mkfs_simple_time(x) time(x)

static void super_set_state(void)
{
	Super.s_state |= SIMPLE_VALID_FS;
	Super.s_state &= ~SIMPLE_ERROR_FS;
}

/**
  * Write boot sector, super-block, inode map, zone map, inodes
*/

static void write_tables(const struct fs_control *ctl) {
	unsigned long imaps = get_nimaps();
	unsigned long zmaps = get_nzmaps();
	size_t buffsz = get_inode_buffer_size();

	/* Mark the super block valid. */
	super_set_state();

	if (lseek(ctl->device_fd, 0, SEEK_SET))
		err(MKFS_EX_ERROR, _("%s: seek to boot block failed "
				   " in write_tables"), ctl->device_name);
	if (write_all(ctl->device_fd, boot_block_buffer, 512))
		err(MKFS_EX_ERROR, _("%s: unable to clear boot sector"), ctl->device_name);
	if (SIMPLE_BLOCK_SIZE != lseek(ctl->device_fd, SIMPLE_BLOCK_SIZE, SEEK_SET))
		err(MKFS_EX_ERROR, _("%s: seek failed in write_tables"), ctl->device_name);

	if (write_all(ctl->device_fd, super_block_buffer, SIMPLE_BLOCK_SIZE))
		err(MKFS_EX_ERROR, _("%s: unable to write super-block"), ctl->device_name);

	if (write_all(ctl->device_fd, inode_map, imaps * SIMPLE_BLOCK_SIZE))
		err(MKFS_EX_ERROR, _("%s: unable to write inode map"), ctl->device_name);

	if (write_all(ctl->device_fd, zone_map, zmaps * SIMPLE_BLOCK_SIZE))
		err(MKFS_EX_ERROR, _("%s: unable to write zone map"), ctl->device_name);

	if (write_all(ctl->device_fd, inode_buffer, buffsz))
		err(MKFS_EX_ERROR, _("%s: unable to write inodes"), ctl->device_name);
}

static void write_block(const struct fs_control *ctl, int blk, char * buffer) {
	if (blk * SIMPLE_BLOCK_SIZE != lseek(ctl->device_fd, blk * SIMPLE_BLOCK_SIZE, SEEK_SET))
		errx(MKFS_EX_ERROR, _("%s: seek failed in write_block"), ctl->device_name);

	if (write_all(ctl->device_fd, buffer, SIMPLE_BLOCK_SIZE))
		errx(MKFS_EX_ERROR, _("%s: write failed in write_block"), ctl->device_name);
}

static int get_free_block(struct fs_control *ctl) {
	unsigned int blk;
	unsigned int zones = get_nzones();
	unsigned int first_zone = get_first_zone();

	if (ctl->fs_used_blocks + 1 >= MAX_GOOD_BLOCKS)
		errx(MKFS_EX_ERROR, _("%s: too many bad blocks"), ctl->device_name);
	if (ctl->fs_used_blocks)
		blk = good_blocks_table[ctl->fs_used_blocks - 1] + 1;
	else
		blk = first_zone;
	while (blk < zones && zone_in_use(blk))
		blk++;
	if (blk >= zones)
		errx(MKFS_EX_ERROR, _("%s: not enough good blocks"), ctl->device_name);
	good_blocks_table[ctl->fs_used_blocks] = blk;
	ctl->fs_used_blocks++;
	return blk;
}

static void mark_good_blocks(const struct fs_control *ctl) {
	int blk;

	for (blk=0 ; blk < ctl->fs_used_blocks ; blk++)
		mark_zone(good_blocks_table[blk]);
}


static inline int next(unsigned long zone) {
	unsigned long zones = get_nzones();
	unsigned long first_zone = get_first_zone();

	if (!zone)
		zone = first_zone-1;
	while (++zone < zones)
		if (zone_in_use(zone))
			return zone;
	return 0;
}


static void make_bad_inode (struct fs_control *ctl)
{
	#define NEXT_BAD (zone = next(zone))
	struct simple_inode *inode = &Inode[SIMPLE_BAD_INO];
	int i, j, zone;
	int ind = 0, dind = 0;
	unsigned long ind_block[SIMPLE_BLOCK_SIZE >> 2];
	unsigned long dind_block[SIMPLE_BLOCK_SIZE >> 2];

	if (!ctl->fs_bad_blocks)
		return;
	mark_inode (SIMPLE_BAD_INO);
	inode->i_nlinks = 1;
	inode->i_atime = inode->i_mtime = inode->i_ctime = mkfs_minix_time(NULL);
	inode->i_mode = S_IFREG + 0000;
	inode->i_size = ctl->fs_bad_blocks * SIMPLE_BLOCK_SIZE;
	zone = next (0);
	for (i = 0; i < 7; i++) {
		inode->i_zone[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[7] = ind = get_free_block (ctl);
	memset (ind_block, 0, SIMPLE_BLOCK_SIZE);
	for (i = 0; i < 256; i++) {
		ind_block[i] = zone;
		if (!NEXT_BAD)
			goto end_bad;
	}
	inode->i_zone[8] = dind = get_free_block (ctl);
	memset (dind_block, 0, SIMPLE_BLOCK_SIZE);
	for (i = 0; i < 256; i++) {
		write_block (ctl, ind, (char *) ind_block);
		dind_block[i] = ind = get_free_block (ctl);
		memset (ind_block, 0, SIMPLE_BLOCK_SIZE);
		for (j = 0; j < 256; j++) {
			ind_block[j] = zone;
			if (!NEXT_BAD)
				goto end_bad;
		}
	}
	/* Could make triple indirect block here */
	errx(MKFS_EX_ERROR, _("%s: too many bad blocks"), ctl->device_name);
 end_bad:
	if (ind)
		write_block (ctl, ind, (char *) ind_block);
	if (dind)
		write_block (ctl, dind, (char *) dind_block);
}

static void make_root_inode_v2_v3 (struct fs_control *ctl) {
	char *tmp = root_block;

	*(uint16_t *) tmp = 1;
	strcpy(tmp + 2, ".");
	tmp += ctl->fs_dirsize;
	*(uint16_t *) tmp = 1;
	strcpy(tmp + 2, "..");
	tmp += ctl->fs_dirsize;
	*(uint16_t *) tmp = 2;
	strcpy(tmp + 2, ".badblocks");
    
	struct simple_inode *inode = &Inode[SIMPLE_ROOT_INO];

	mark_inode (SIMPLE_ROOT_INO);
	inode->i_zone[0] = get_free_block (ctl);
	inode->i_nlinks = 2;
	inode->i_atime = inode->i_mtime = inode->i_ctime = mkfs_simple_time(NULL);

	if (ctl->fs_bad_blocks)
		inode->i_size = 3 * ctl->fs_dirsize;
	else {
		memset(&root_block[2 * ctl->fs_dirsize], 0, ctl->fs_dirsize);
		inode->i_size = 2 * ctl->fs_dirsize;
	}

	inode->i_mode = S_IFDIR + 0755;
	inode->i_uid = getuid();
	if (inode->i_uid)
		inode->i_gid = getgid();
	write_block (ctl, inode->i_zone[0], root_block);
}

static void super_set_nzones(const struct fs_control *ctl)
{
	Super.s_zones = ctl->fs_blocks;
}

static void super_init_maxsize(void)
{
	Super.s_max_size =  0x7fffffff;
}

static void super_set_map_blocks(const struct fs_control *ctl, unsigned long inodes)
{
	Super.s_imap_blocks = UPPER(inodes + 1, BITS_PER_BLOCK);
	Super.s_zmap_blocks = UPPER(ctl->fs_blocks - (1 + get_nimaps() + inode_blocks()),
				     BITS_PER_BLOCK + 1);
	Super.s_firstdatazone = first_zone_data();
}

static void super_set_magic(const struct fs_control *ctl)
{
	Super.s_magic = ctl->fs_magic;
}

static void setup_tables(const struct fs_control *ctl) {
	unsigned long inodes, zmaps, imaps, zones, i;

	super_block_buffer = xcalloc(1, SIMPLE_BLOCK_SIZE);

	memset(boot_block_buffer,0,512);
	super_set_magic(ctl);

	Super.s_log_zone_size = 0;

	super_init_maxsize();
	super_set_nzones(ctl);
	zones = get_nzones();

	/* some magic nrs: 1 inode / 3 blocks for smaller filesystems,
	 * for one inode / 16 blocks for large ones. mkfs will eventually
	 * crab about too far when getting close to the maximum size. */
	if (ctl->fs_inodes == 0)
		if (2048 * 1024 < ctl->fs_blocks)	/* 2GB */
			inodes = ctl->fs_blocks / 16;
		else if (512 * 1024 < ctl->fs_blocks)	/* 0.5GB */
			inodes = ctl->fs_blocks / 8;
		else
			inodes = ctl->fs_blocks / 3;
	else
		inodes = ctl->fs_inodes;
	/* Round up inode count to fill block size */
	inodes = ((inodes + SIMPLE_INODES_PER_BLOCK - 1) &
			~(SIMPLE_INODES_PER_BLOCK - 1));


	if (inodes > SIMPLE_MAX_INODES)
		inodes = SIMPLE_MAX_INODES;
	Super.s_ninodes = inodes;

	super_set_map_blocks(ctl, inodes);
	if (SIMPLE_MAX_INODES < first_zone_data())
		errx(MKFS_EX_ERROR,
		     _("First data block at %jd, which is too far (max %d).\n"
		       "Try specifying fewer inodes by passing --inodes <num>"),
		     (intmax_t)first_zone_data(),
		     SIMPLE_MAX_INODES);
	imaps = get_nimaps();
	zmaps = get_nzmaps();

	inode_map = xmalloc(imaps * SIMPLE_BLOCK_SIZE);
	zone_map = xmalloc(zmaps * SIMPLE_BLOCK_SIZE);
	memset(inode_map,0xff,imaps * SIMPLE_BLOCK_SIZE);
	memset(zone_map,0xff,zmaps * SIMPLE_BLOCK_SIZE);

	for (i = get_first_zone() ; i<zones ; i++)
		unmark_zone(i);
	for (i = SIMPLE_ROOT_INO ; i<=inodes; i++)
		unmark_inode(i);

	inode_buffer = xmalloc(get_inode_buffer_size());
	memset(inode_buffer,0, get_inode_buffer_size());

	printf(P_("%lu inode\n", "%lu inodes\n", inodes), inodes);
	printf(P_("%lu block\n", "%lu blocks\n", zones), zones);
	printf(_("Firstdatazone=%jd (%jd)\n"),
		(intmax_t)get_first_zone(), (intmax_t)first_zone_data());
	printf(_("Zonesize=%zu\n"), (size_t) SIMPLE_BLOCK_SIZE << get_zone_size());
	printf(_("Maxsize=%zu\n\n"),get_max_size());
}

/*
 * Perform a test of a block; return the number of
 * blocks readable/writable.
 */
static size_t do_check(const struct fs_control *ctl, char * buffer, int try, unsigned int current_block) {
	ssize_t got;

	/* Seek to the correct loc. */
	if (lseek(ctl->device_fd, current_block * SIMPLE_BLOCK_SIZE, SEEK_SET) !=
		       current_block * SIMPLE_BLOCK_SIZE )
		err(MKFS_EX_ERROR, _("%s: seek failed during testing of blocks"),
				ctl->device_name);

	/* Try the read */
	got = read(ctl->device_fd, buffer, try * SIMPLE_BLOCK_SIZE);
	if (got < 0) got = 0;
	if (got & (SIMPLE_BLOCK_SIZE - 1 )) {
		printf(_("Weird values in do_check: probably bugs\n"));
	}
	got /= SIMPLE_BLOCK_SIZE;
	return got;
}

static unsigned int currently_testing = 0;

static void alarm_intr(int alnum __attribute__ ((__unused__))) {
	unsigned long zones = get_nzones();

	if (currently_testing >= zones)
		return;
	signal(SIGALRM,alarm_intr);
	alarm(5);
	if (!currently_testing)
		return;
	printf("%d ...", currently_testing);
	fflush(stdout);
}

static void check_blocks(struct fs_control *ctl) {
	size_t try, got;
	static char buffer[SIMPLE_BLOCK_SIZE * TEST_BUFFER_BLOCKS];
	unsigned long zones = get_nzones();
	unsigned long first_zone = get_first_zone();

	currently_testing=0;
	signal(SIGALRM,alarm_intr);
	alarm(5);
	while (currently_testing < zones) {
		if (lseek(ctl->device_fd, currently_testing * SIMPLE_BLOCK_SIZE,SEEK_SET) !=
		    currently_testing*SIMPLE_BLOCK_SIZE)
			errx(MKFS_EX_ERROR, _("%s: seek failed in check_blocks"),
					ctl->device_name);
		try = TEST_BUFFER_BLOCKS;
		if (currently_testing + try > zones)
			try = zones-currently_testing;
		got = do_check(ctl, buffer, try, currently_testing);
		currently_testing += got;
		if (got == try)
			continue;
		if (currently_testing < first_zone)
			errx(MKFS_EX_ERROR, _("%s: bad blocks before data-area: "
					"cannot make fs"), ctl->device_name);
		mark_zone(currently_testing);
		ctl->fs_bad_blocks++;
		currently_testing++;
	}
	if (ctl->fs_bad_blocks > 0)
		printf(P_("%d bad block\n", "%d bad blocks\n", ctl->fs_bad_blocks), ctl->fs_bad_blocks);
}

static void get_list_blocks(struct fs_control *ctl, char *filename) {
	FILE *listfile;
	unsigned long blockno;

	listfile = fopen(filename,"r");
	if (listfile == NULL)
		err(MKFS_EX_ERROR, _("%s: can't open file of bad blocks"),
				ctl->device_name);

	while (!feof(listfile)) {
		if (fscanf(listfile,"%lu\n", &blockno) != 1) {
			printf(_("badblock number input error on line %d\n"), ctl->fs_bad_blocks + 1);
			errx(MKFS_EX_ERROR, _("%s: cannot read badblocks file"),
					ctl->device_name);
		}
		mark_zone(blockno);
		ctl->fs_bad_blocks++;
	}
	fclose(listfile);

	if (ctl->fs_bad_blocks > 0)
		printf(P_("%d bad block\n", "%d bad blocks\n", ctl->fs_bad_blocks), ctl->fs_bad_blocks);
}

static int find_super_magic(const struct fs_control *ctl)
{
	return SIMPLE_SUPER_MAGIC;
}

static void check_user_instructions(struct fs_control *ctl)
{
	ctl->fs_magic = find_super_magic(ctl);
}

int main(int argc, char ** argv)
{
	struct fs_control ctl = {
		.fs_namelen = 30,	/* keep in sync with DEFAULT_FS_VERSION */
		0
	};
	int i;
	struct stat statbuf;
	char * listfile = NULL;
	enum {
		OPT_LOCK = CHAR_MAX + 1
	};
	static const struct option longopts[] = {
		{"inodes", required_argument, NULL, 'i'},
		{"check", no_argument, NULL, 'c'},
		{"badblocks", required_argument, NULL, 'l'},
		{"help", no_argument, NULL, 'h'},
		{"lock",optional_argument, NULL, OPT_LOCK},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	strutils_set_exitcode(MKFS_EX_USAGE);

	while ((i = getopt_long(argc, argv, "n:i:cl:Vh", longopts, NULL)) != -1)
		switch (i) {
		case 'i':
			ctl.fs_inodes = strtoul_or_err(optarg,
					_("failed to parse number of inodes"));
			break;
		case 'c':
			ctl.check_blocks = 1;
			break;
		case 'l':
			listfile = optarg;
			break;
		case OPT_LOCK:
			ctl.lockmode = "1";
			if (optarg) {
				if (*optarg == '=')
					optarg++;
				ctl.lockmode = optarg;
			}
			break;
		case 'h':
			usage();
		default:
			errtryhelp(MKFS_EX_USAGE);
		}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		ctl.device_name = argv[0];
		argc--;
		argv++;
	}
	if (argc > 0)
		ctl.fs_blocks = strtoul_or_err(argv[0], _("failed to parse number of blocks"));


    /* device_name given? */
	if (!ctl.device_name) {
		warnx(_("no device specified"));
		errtryhelp(MKFS_EX_USAGE);
	}
	check_user_instructions(&ctl);  /* check arguments */
    /* already mounted? */
	if (is_mounted(ctl.device_name))
		errx(MKFS_EX_ERROR, _("%s is mounted; will not make a filesystem here!"),
			ctl.device_name);
    /* stat works? */
	if (stat(ctl.device_name, &statbuf) < 0)
		err(MKFS_EX_ERROR, _("stat of %s failed"), ctl.device_name);
	ctl.device_fd = open_blkdev_or_file(&statbuf, ctl.device_name, O_RDWR);
	if (ctl.device_fd < 0)
		err(MKFS_EX_ERROR, _("cannot open %s"), ctl.device_name);
	if (blkdev_lock(ctl.device_fd, ctl.device_name, ctl.lockmode) != 0)
		exit(MKFS_EX_ERROR);
	determine_device_blocks(&ctl, &statbuf);
	setup_tables(&ctl);
	if (ctl.check_blocks)
		check_blocks(&ctl);
	else if (listfile)
		get_list_blocks(&ctl, listfile);

	make_root_inode(&ctl);
	make_bad_inode(&ctl);

	mark_good_blocks(&ctl);
	write_tables(&ctl);
	if (close_fd(ctl.device_fd) != 0)
		err(MKFS_EX_ERROR, _("write failed"));

	return MKFS_EX_OK;
}
