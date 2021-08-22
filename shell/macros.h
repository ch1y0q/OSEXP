#define _GNU_SOURCE

#ifndef MACROS_H
#define MACROS_H

/* debug */
//#define DEBUG
#define WERR(x) write(STDERR_FILENO, x, strlen(x));
#define WOUT(x) write(STDOUT_FILENO, x, strlen(x));

/* boolean */
#define BOOL int
#define TRUE 1
#define FALSE 0

/* error message */
#define PRINT_ERROR_MESSAGE WERR("An error has occurred\n")

/* environment */
#define MAX_STRCMP_N 1000
#define MAX_PATH 100     /* max length of a path */
#define MAX_PATH_NUM 100 /* max number of paths */
#define MAX_PIDS 500
#define MAX_ARGUMENTS 100
#define MAX_CWD 500
#define MAX_REDIRECTION_SEP 5
#define MAX_EXEC_FILENAME 300
#define DEFAULT_PATHS "/bin"

/* exit */
#define EXIT_SUCCESS 0
#define EXIT_BATCH_READ_ERROR 1
#define EXIT_EXEC 2

#endif