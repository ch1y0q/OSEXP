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

char *getNextLine(FILE *batchFile)
{
    char *line = NULL;
    size_t len = 0;
    if (batchFile == NULL)
    {
        write(STDOUT_FILENO, "seush>", strlen("seush>"));
        while (getline(&line, &len, stdin) == -1)
        {
            /* wait for input */
        };
    }
    else if (getline(&line, &len, batchFile) == -1)
    {
        free(line);
        return NULL;
    }
    return line;
}

void run_shell(char *batch)
{
    /* initialize environment */
    Environment environment;
    environment.paths[0] = strdup(DEFAULT_PATHS);
    environment.paths[1] = strdup("\0");
    environment.cwd[0] = '.';
    environment.cwd[1] = '\0';
    environment.path_set_by_user = FALSE;

    FILE *batchFile = NULL;
    if (batch != NULL)
    { /* read from file*/
        batchFile = fopen(batch, "r");
        if (batchFile == NULL)
        {
            PRINT_ERROR_MESSAGE;
            exit(1);
        }
    }

    char *line;
    while (TRUE)
    {
        line = getNextLine(batchFile);
#ifdef DEBUG
        //printf("%s\n", line);
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
        if (strncmp(line, "exit", 4) == 0)
        {
            if (line[4] == '\0')
            {
                free(line);
                exit(0);
            }
            else
            {
                PRINT_ERROR_MESSAGE;
            }
        }
        else if (strncmp(line, "path", 4) == 0)
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
        else if (strncmp(line, "cd", 2) == 0)
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
