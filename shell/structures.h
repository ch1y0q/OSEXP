#define _GNU_SOURCE
#include <linux/limits.h>
#include <limits.h>
#include <sys/types.h>
#include <stdio.h>
#include "macros.h"

#ifndef STRUCTURES_H
#define STRUCTURES_H

#define REDI_IN_MASK 1
#define REDI_OUT_MASK 2
#define REDI_IN_OUT_ORDER_MASK 4
#define REDI_PIPE_MASK 8

enum REDIRECTION_TYPE{
    NO_REDI=0b0000, REDI_IN=0b0001, REDI_OUT=0b0010, REDI_OUT_IN=0b0011, REDI_IN_OUT=0b0111,
//    REDI_PIPE_IN=0b1001, REDI_PIPE_OUT=0b1010, REDI_PIPE_OUT_IN=0b1011, REDI_PIPE_IN_OUT=0b1111,
};

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
    enum REDIRECTION_TYPE redirected;
    int redi_infd; // fd of opened infile
    int redi_outfd; // fd of opened outfile
} Process;


#endif