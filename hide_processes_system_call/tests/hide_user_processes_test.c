/**

This program tests `hide_user_processes` system call.

**/

#include<stdio.h>
#include<sys/syscall.h>
#include<unistd.h>

#define SYSCALL_NUM 448
#define MY_UID 1000

int main()
{
    int syscallNum = SYSCALL_NUM;
    uid_t uid = MY_UID;
    char *binname = "bash";
    int recover = 1;
    syscall(syscallNum,uid,binname,recover);
    return 0;
}

