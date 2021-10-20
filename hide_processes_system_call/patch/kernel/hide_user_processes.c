/**
 * MODIFIED
 * Implementation of system call `hide_user_processes`
*/

#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/proc_fs.h>
#include <linux/cred.h>
#include <linux/string.h>

SYSCALL_DEFINE3(hide_user_processes, uid_t, uid, char *, binname, int, recover)
{
	if (current_uid().val != 0) { /* only root can call */
		printk("Permission denied. Only root can call hide_user_processes.\n");
		return 1;
	}

	struct task_struct *p = NULL;
	if (recover == 0) /* recover = 0: allow root to hide processes */
	{
		if (binname == NULL)
		/* if null, hide all processes of the given uid */
		{
			for_each_process (p) {
				if (p->cred->uid.val == uid) {
					p->cloak = 1;
					proc_flush_pid(get_pid(p->thread_pid));
				}
			}
			printk("All processes of uid %d are hidden.\n", uid);
		} else /* otherwise, hide the process with the given name */
		{
			char kbinname[TASK_COMM_LEN];
    		kbinname[TASK_COMM_LEN - 1] = '\0';
			long len = strncpy_from_user(kbinname, binname, TASK_COMM_LEN);
			if(unlikely(len < 0)){	/* unable to copy from user space */
				printk("Unable to do strncpy_from_user");
				return 2;
			}
			for_each_process (p) {
				char s[TASK_COMM_LEN];
                get_task_comm(s, p);  /* get name("comm") of process */
				if (p->cred->uid.val == uid && strncmp(s, kbinname, TASK_COMM_LEN) == 0) {
					p->cloak = 1;
					printk("Process %s of uid %d is hidden.\n",
					       kbinname, uid);
					proc_flush_pid(get_pid(p->thread_pid));
				}
			}
		}
	}

	/* recover != 0: display all of the processes, including previously hidden ones */
	else {
		for_each_process (p) {
			p->cloak = 0;
		}
	}

	return 0;
}