/**
 * MODIFIED
 * Add procfs entry `hidden_process`
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>

#include <linux/var_defs.h>

#define PRINT_KERNEL_MESSAGE

static struct proc_dir_entry *hidden_process_entry;

static ssize_t proc_read_hidden_process(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	ssize_t cnt;
	ssize_t ret;
	char kbuf[1000];
	char tmp[16];
	struct task_struct *p;
#ifdef PRINT_KERNEL_MESSAGE
	printk("In proc_read_hidden_process.\n");
#endif
	sprintf(kbuf, "%s", "");	/* init buffer */
	for_each_process (p) {
		if (p->cloak == 1) {
			sprintf(tmp, "%ld ", (long)p->pid);
			strcat(kbuf, tmp);
		}
	}
	cnt = strlen(kbuf);

	/* ret contains the amount of chare wasn't successfully written to `buf` */
	ret = copy_to_user(buf, kbuf, cnt);
	*ppos += cnt - ret;

	/* Making sure there are no left bytes of data to send user */
	if (*ppos > cnt)
		return 0;
	else
		return cnt;
}

static const struct proc_ops hidden_process_proc_ops = {
	.proc_read = proc_read_hidden_process,
};

static int __init proc_hidden_process_init(void)
{
	/* 0:oct 6:rw 4:r */
	hidden_process_entry = proc_create("hidden_process", 0444, NULL,
				 &hidden_process_proc_ops);
	return 0;
}

void proc_hidden_process_cleanup(void)
{
	proc_remove(hidden_process_entry);
}
fs_initcall(proc_hidden_process_init);

/** if you'd like to build it as a module
MODULE_LICENSE("GPL");
MODULE_AUTHOR("HUANG Qiyue <qiyue2001@gmail.com>");
MODULE_DESCRIPTION("Simple hidden_process process driver (procfs)");
MODULE_VERSION("1.0");
module_init(proc_hidden_process_init);
module_exit(proc_hidden_process_cleanup);
*/
