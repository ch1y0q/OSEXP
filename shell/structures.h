#define _GNU_SOURCE
#include <linux/limits.h>
#include <limits.h>
#include <sys/types.h>
#include <stdio.h>
#include "macros.h"

#ifndef STRUCTURES_H
#define STRUCTURES_H

typedef struct Environment      /*run-time context*/
{
    char *paths[MAX_PATH_NUM]; // array of paths to be searched
    char cwd[MAX_PATH];        // current working directory
    BOOL path_set_by_user;     // if the path was set by user
} Environment;

typedef struct Process
{
    pid_t pid;
    int argc;    // number of arguments
    char **argv; // array of char*
    char *exec_path;
    BOOL redirected;
    int redirection; // fd of opened file
} Process;

#endif