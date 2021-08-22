#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "macros.h"
#include "structures.h"
#include "utilities.h"
#include "run_command.h"


void print_promt(struct Environment *environment)
{
    char str[2048]="";
    char buf[1024];
    /* ANSI colored string: green */
    strcat(str, "\e[32;40m");
    /* get username */
    if(getlogin_r(buf, 1024)){
        PRINT_ERROR_MESSAGE;
    }
    strcat(str, buf);

    strcat(str, "@");

    /* get hostname */
    if(gethostname(buf, 1024)){
        PRINT_ERROR_MESSAGE;
    }
    strcat(str, buf);
    
    /* append a colon in white */
    strcat(str, "\033[0m:");
    
    /* append cwd in red */
    strcat(str, "\033[0;31m");
    strncat(str, environment->cwd, MAX_PATH);

    if(geteuid() != 0)  /* not root */
    {
      strcat(str, "\033[0m$ ");
    }
    else{
      strcat(str, "\033[0m# ");
    }

    WOUT(str);
}

char *getNextLine(FILE *batchFile, struct Environment* environment)
{
    char *line = NULL;
    size_t len = 0;
    if (batchFile == NULL)  /* read from stdin */
    {
        print_promt(environment);
        while (getline(&line, &len, stdin) == -1)
        {
            /* wait for input */
        };
    }
    else if (getline(&line, &len, batchFile) == -1) /* read next line from batch file*/
    {
        free(line);
        return NULL;
    }
    return line;
}

void run_shell(char *batch)
{
    /* initialize environment */
    struct Environment environment;
    environment.paths[0] = strdup(DEFAULT_PATHS);
    environment.paths[1] = strdup("\0");
    getcwd(environment.cwd, MAX_CWD);
    //environment.cwd[0] = '.';
    //environment.cwd[1] = '\0';
    environment.path_set_by_user = FALSE;

    FILE *batchFile = NULL;
    if (batch != NULL)
    { /* read from file*/
        batchFile = fopen(batch, "r");
        if (batchFile == NULL)
        {
            PRINT_ERROR_MESSAGE;
            exit(EXIT_BATCH_READ_ERROR);
        }
    }

    /*====================================*/
    /*             MAIN  LOOP             */
    /*====================================*/
    char *line;
    while (TRUE)
    {
        line = getNextLine(batchFile, &environment);
#ifdef DEBUG
        printf("%s\n", line);
#endif

        if (line == NULL || line[0] == '\0')
        {
            if (batchFile != NULL)
                break;
            else
                continue;
        }

        line = clean(line);

        /* built-in commands */
        /* not to run in parallel */
        if (strncmp(line, "exit", MAX_STRCMP_N) == 0)
        {
            free(line);
            exit(EXIT_SUCCESS);
        }
        else if (strncmp(line, "help", MAX_STRCMP_N) == 0)
        {
            WOUT("A simple shell.\n");
        }
        else if ((strlen(line) == 4 && strncmp(line, "path", 4) == 0)
                || strncmp(line, "path ", 5)==0)   /* "path" or "path <paths>" */
        {
            /* clear previous path if exists */
            int _i = 0;
            while (environment.paths[_i] && environment.paths[_i][0] != '\0')
            {
                free(environment.paths[_i]);
                _i++;
            }

            char *paths = strdup(line + 4); // skip "path"
            environment.path_set_by_user = TRUE;
            int path_sep_num = 0;
            if (paths[0] != '\0')
            {
                char *token = strtok(paths, " ");
                while (token != NULL)
                {
                    environment.paths[path_sep_num++] = strdup(token);
                    token = strtok(NULL, " ");
                }
            }
            environment.paths[path_sep_num] = strdup("");
        }
        else if (strncmp(line, "cd ", 3) == 0)
        {
            char *new_dir = line + 3;
            DIR *dir = opendir(new_dir);
            if (dir)
            {
                strncpy(environment.cwd, new_dir, MAX_PATH);
                chdir(new_dir);
                closedir(dir);
            }
            else
            {
                PRINT_ERROR_MESSAGE;
            }
        }

        /* run a non-built-in command */
        else
        {
            run_command(line, &environment);
        }

        free(line);
    }

    /* close batch file */
    if (batchFile != NULL)
    {
        fclose(batchFile);
    }
}
