#define _GNU_SOURCE
#ifndef RUN_COMMAND_H
#define RUN_COMMAND_H

#include "structures.h"

void parse_command(char *, Environment *);
void run_command(char *, Environment *);
void run_processes(struct Process process[], int process_num);

#endif