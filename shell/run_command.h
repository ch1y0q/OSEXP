#define _GNU_SOURCE
#ifndef RUN_COMMAND_H
#define RUN_COMMAND_H

#include "structures.h"

int fd[2]; /* fd[0]: read end, fd[1] write end */
int save_in, save_out;

void parse_command(char *, Environment *);
void run_command(char *, Environment *);
void run_process(struct Process *process);
void run_processes(struct Process process[], int process_num);
void handle_redirection(char* line, struct Process process[], int process_num, struct Environment* environment);
//void handle_redirection_exec(char *line, enum REDIRECTION_TYPE type, int infd, int outfd, struct Environment *environment);
void handle_redirection_exec(char *line, struct Environment *environment);
char *handle_pipe(char* line, struct Environment* environment, BOOL);

#endif