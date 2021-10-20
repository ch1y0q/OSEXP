/**

This program tests `hide` system call.

**/


#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSCALL_NUM 447

int
main()
{
    pid_t pid = 1;
    int on = 1;
    syscall(SYSCALL_NUM, pid, on);
    return 0;
}
