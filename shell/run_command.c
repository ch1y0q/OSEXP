#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#include "macros.h"
#include "run_command.h"
#include "utilities.h"
#include "structures.h"

void run_command(char *line, Environment *environment)
{
    parse_command(line, environment);
}

void parse_command(char *line, Environment *environment)
{
    //Process process[MAX_PIDS] = {0}; /* TROUBLESHOOTING: must initialize or will reuse */
    /* allocation and initialization of Process */
    Process *process = malloc(sizeof(Process) * MAX_PIDS);
    for(int _i = 0; _i < MAX_PIDS; ++_i){
        process[_i].argc = 0;
        process[_i].argv = NULL;
        process[_i].exec_path = NULL;
        process[_i].pid = -1;
        process[_i].redirected = NO_REDI;
        process[_i].redi_infd = -1;
        process[_i].redi_outfd = -1;
    }
    int process_num = 0;

    /* remove redundant space and tab */
    line = clean(line);

    /* parse "&" */
    char **single_commands = malloc(sizeof(char*) * MAX_ARGUMENTS_NUM);
    int single_command_num = 0;
    char *token = strtok(line, "&");
    while (token != NULL)
    {
        single_commands[single_command_num++] = strdup(token);
        token = strtok(NULL, "&");
    }
    single_commands[single_command_num] = NULL;

#ifdef DEBUG
    for (int _i = 0; _i < single_command_num; ++_i)
    {
        printf("%s\n", single_commands[_i]);
    }
#endif

    for (int _i = 0; _i < single_command_num; ++_i)
    {
        char *cur_command = strdup(single_commands[_i]);
        cur_command = clean(cur_command);
        if (cur_command == NULL || cur_command[0] == '\0')
            continue;
        free(cur_command);

        /* pipe */
        /* TODO */

        /* redirection */
        handle_redirection(single_commands[_i], process, process_num, environment);


        /* ready for a new process */
        ++process_num;

        /* free memory */

    }

    chdir(environment->cwd); /* test7: prevent cwd changed by external sh */
    run_processes(process, process_num);

    /* free memory */
    for (int _i = 0; _i < single_command_num; ++_i){
        free(single_commands[_i]);
    }
    free(single_commands);
    for (int _i = 0; _i < process_num; ++_i){
        for (int _j = 0; _j < process[_i].argc; ++_j){
            free(process[_i].argv[_j]);
        }
        free(process[_i].argv);
        free(process[_i].exec_path);
    }
    free(process);
}

void handle_redirection(char* line, struct Process process[], int process_num, struct Environment* environment)
{
        char *token;
        char command_noredi[MAX_COMMAND_NOREDI];
        char redi_outfile[MAX_REDIFILE];
        char redi_infile[MAX_REDIFILE];
        enum REDIRECTION_TYPE redi_type;


        if(3 == sscanf(line, "%[^\t\n<>] > %[^\t\n<>] < %[^\t\n<>]",
                     command_noredi, redi_outfile, redi_infile)){
            redi_type = REDI_OUT_IN;
        }
        else if(3 == sscanf(line, "%[^\t\n<>] < %[^\t\n<>] > %[^\t\n<>]",
                     command_noredi, redi_infile, redi_outfile)){
            redi_type = REDI_IN_OUT;
        }
        else if(2 == sscanf(line, "%[^\t\n<>] < %[^\t\n<>]", command_noredi, redi_infile)){
            redi_type = REDI_IN;
        }
        else if(2 == sscanf(line, "%[^\t\n<>] > %[^\t\n<>]", command_noredi, redi_outfile)){
            redi_type = REDI_OUT;
        }
        else{
            redi_type = NO_REDI;
        }

        process[process_num].redirected=redi_type;
        if(redi_type & REDI_OUT_MASK){
            process[process_num].redi_outfd = open(redi_outfile, O_WRONLY | O_CREAT, 0666);
        }
        if(redi_type & REDI_IN_MASK){
            process[process_num].redi_infd = open(redi_infile, O_RDONLY, 0444);
        }

        /* arguments */
        process[process_num].argv = malloc(sizeof(char *) * MAX_ARGUMENTS_NUM);
        process[process_num].argv[0] = '\0';
        process[process_num].argc = 0;

        token = strtok(clean(command_noredi), " ");
        while (token != NULL)
        {
            process[process_num].argv[process[process_num].argc++] = strdup(token);
            token = strtok(NULL, " ");
        }

        /* find path for command */
        if (access(process[process_num].argv[0], X_OK) == 0) /* absolute path given */
        {
            process[process_num].exec_path = strdup(process[process_num].argv[0]);
        }

        else /* finding from environment paths */
        {
#ifdef DEBUG
            for (int i = 0; environment->paths[i] && environment->paths[i][0] != '\0'; ++i)
            {
                printf("Env: %d %s\n", i, environment->paths[i]);
            }
#endif
            int path_i = 0;
            while (environment->paths[path_i] != NULL && environment->paths[path_i][0] != '\0')
            {
#ifdef DEBUG
                printf("proc %d %s\n", process_num, process[process_num].argv[0]);
#endif
                /* https://stackoverflow.com/questions/12591074
                    /is-there-a-neat-way-to-do-strdup-followed-by-strcat */
                char *full_path = (char*)malloc( sizeof(char) * 
                    (strlen(environment->paths[path_i]) + MAX_EXEC_FILENAME) ) ;
                strcpy(full_path, environment->paths[path_i]);
                if (full_path[strlen(full_path) - 1] != '/')
                {
                    strcat(full_path, "/");
                }
                strcat(full_path, process[process_num].argv[0]);

                if (access(full_path, X_OK) == 0)
                {
                    process[process_num].exec_path = strdup(full_path);
                    break;
                }
                path_i++;
                free(full_path);
            }
        }
}

/* run all processes and wait for them to finnish */
void run_processes(struct Process process[], const int process_num )
{
    pid_t pid = 0, wpid;
    int wait_status;

    for (int number = 0; number < process_num; number++)
    {
#ifdef DEBUG
        printf("%d: %s %d %d \n", number, process[number].exec_path, process[number].argc, process[number].redirected);
#endif
        pid = process[number].pid = fork();

        if (pid < 0)
        {
            PRINT_ERROR_MESSAGE;
            exit(1);
        }

        else if (pid == 0)
        { /* forked process */
#ifdef DEBUG
            printf("pid: %d pgrp: %d redi_type: %d\n", getpid(), getpgrp(), process[number].redirected);
#endif
            if (process[number].redirected & REDI_OUT_MASK)
            { /* handle redirection */
                if (-1 == dup2(process[number].redi_outfd, fileno(stderr)))
                {
                    PRINT_ERROR_MESSAGE;
                    exit(1);
                }
                /* close(1)  i.e. STDOUT is done by dup2 */
                if (-1 == dup2(process[number].redi_outfd, fileno(stdout)))
                {
                    PRINT_ERROR_MESSAGE;
                    exit(1);
                }
                #ifdef DEBUG
                    printf("dup2 stdout and stderr...\n");
                #endif
            }
            if (process[number].redirected & REDI_IN_MASK)
            { /* handle redirection */
                if (-1 == dup2(process[number].redi_infd, fileno(stdin)))
                {
                    #ifdef DEBUG
                    printf("dup2 stdin...\n");
                    #endif
                    PRINT_ERROR_MESSAGE;
                    exit(1);
                }
            }
            execv(process[number].exec_path, process[number].argv);
            PRINT_ERROR_MESSAGE;
            #ifdef DEBUG
            printf("execv didn't work.\n");
            #endif
            exit(EXIT_EXEC); /* something went wrong executing subprocess */
        }
    }
    if (pid) /* parent process */
    {
        while ((wpid = wait(&wait_status)) > 0)
            ;
        /* https://stackoverflow.com/a/23872806/4810608 */
    }
}