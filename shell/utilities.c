#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "structures.h"
#include "macros.h"

/* clean redundant space and tab before command and arguments */
char *clean(char *line)
{
    if(!line) return NULL;
    size_t len = strlen(line), i = 0;
    int character = 0;
    int lastNonSpace = -1;
    while (line[i] == ' ' || line[i] == '\t')
    { /* remove initial spaces*/
        ++i;
    }

    BOOL LAST_IS_SPACE_TAB = FALSE;
    while (line[character + i] != '\0' && line[character + i] != '\n')
    {
        line[character] = line[character + i];
        if (line[character + i] != ' ' && line[character + i] != '\t')
        {
            LAST_IS_SPACE_TAB = FALSE;
            lastNonSpace = character;
        }
        else
        {
            while (line[character + i] == ' ' || line[character + i] == '\t')
            {
                LAST_IS_SPACE_TAB = TRUE;
                ++i;
            }
        }
        if (LAST_IS_SPACE_TAB)
            --i;
        character++;
    }
    line[lastNonSpace + 1] = '\0';
    return line;
}

enum PATH_TYPE
{
    RELATIVE,
    ABSOLUTE,
    INVALID
};

int handle_relative_path(char *path, char *cwd, char *results)
{
    results[0] = '\0';
    if (path[0] != '\0')
    {
        if (path[0] == '.' && path[1] == '/')
        {
            strcat(results, cwd);
            strcat(results, "/");
            strncat(results, path + 2, MAX_PATH - strnlen(cwd, MAX_PATH - 1) - 1);
            return RELATIVE;
        }
        else
        {
            strncat(results, path, MAX_PATH);
            return ABSOLUTE;
        }
    }
    return INVALID;
}

char *to_absolute_path(char *path, char *cwd)
{
    char *results = malloc(MAX_PATH * sizeof(char));
    if (handle_relative_path(path, cwd, results) == RELATIVE && path[0] != '/')
    {
        int cwd_length = strnlen(cwd, MAX_PATH);
        strcpy(results, cwd);
        strcat(results, "/");
        strncat(results, path, MAX_PATH - cwd_length - 1);
        /* remove ending '/' if exists */
        if (results[strnlen(results, MAX_PATH)] == '/')
        {
            results[strnlen(results, MAX_PATH)] = '\0';
        }
    }
    return results;
}
