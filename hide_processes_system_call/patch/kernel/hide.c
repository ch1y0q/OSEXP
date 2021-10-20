/**
 * MODIFIED
 * Implementation of system call `hide`.
 */

#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/proc_fs.h>
#include <linux/cred.h>

SYSCALL_DEFINE2(hide, pid_t, pid, int, on)
{
	printk("Syscall hide called.");
	struct task_struct *p;
	struct pid *thread_pid;
	p = NULL;
	if (pid > 0 && current_uid().val == 0) /* only root can hide process */
	{
		//OBSOLETE: p=find_task_by_pid(pid);
		p = pid_task(find_vpid(pid), PIDTYPE_PID);
		if (!p)
			return 1;
		p->cloak = on; /* set the state of the process */
		if (on == 1) {
			printk("Process %d is hidden by root.\n", pid);
		}
		if (on == 0) {
			printk("Process %d is displayed by root.\n", pid);
		}
		thread_pid = get_pid(p->thread_pid);
		proc_flush_pid(thread_pid);
	} else
		printk("Permission denied. You must be root to hide a process.\n");

	return 0;
}