/**
 * MODIFIED
 * Add procfs entry `hide`
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

#define PROC_MAX_SIZE 16
#define PRINT_KERNEL_MESSAGE

int hidden_flag = 1;

static struct proc_dir_entry *hide_entry;

static ssize_t proc_read_hidden(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	char str[16];
	ssize_t cnt;
	ssize_t ret;
#ifdef PRINT_KERNEL_MESSAGE
	printk("In proc_read_hidden.\n");
	printk("hidden_flag: %d\n", hidden_flag);
#endif
	snprintf(str, sizeof(str), "%d\n", hidden_flag);
	cnt = strlen(str);

	/* ret contains the amount of chare wasn't successfully written to `buf` */
	ret = copy_to_user(buf, str, cnt);
	*ppos += cnt - ret;

	/* Making sure there are no left bytes of data to send user */
	if (*ppos > cnt)
		return 0;
	else
		return cnt;
}

static ssize_t proc_write_hidden(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	char temp[PROC_MAX_SIZE];
	int tmp_flag = 0;
#ifdef PRINT_KERNEL_MESSAGE
	printk("In proc_write_hidden.\n");
#endif
	if (count > PROC_MAX_SIZE)
		count = PROC_MAX_SIZE;
	if (copy_from_user(temp, buf, count)) {
		return -EFAULT;
	}
	//temp[count]='\0';
	if (kstrtoint(temp, 10, &tmp_flag)) /* 10: base */
		return -1;
	hidden_flag = tmp_flag; /* set the value of hidden_flag */
#ifdef PRINT_KERNEL_MESSAGE
	printk("hidden_flag: %d\n", hidden_flag);
#endif
	return count;
}

static const struct proc_ops hide_proc_ops = {
	.proc_write = proc_write_hidden,
	.proc_read = proc_read_hidden,
};

static int __init proc_hide_init(void)
{
	/* 0:oct 6:rw 4:r */
	hide_entry = proc_create("hide", 0644, NULL, &hide_proc_ops);
	return 0;
}

void proc_hide_cleanup(void)
{
	proc_remove(hide_entry);
}
fs_initcall(proc_hide_init);

/** if you'd like to build it as a module
MODULE_LICENSE("GPL");
MODULE_AUTHOR("HUANG Qiyue <qiyue2001@gmail.com>");
MODULE_DESCRIPTION("Simple hide process driver (procfs)");
MODULE_VERSION("1.0");
module_init(proc_hide_init);
module_exit(proc_hide_cleanup);
*/
