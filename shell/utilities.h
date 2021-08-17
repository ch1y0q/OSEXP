#define _GNU_SOURCE
#ifndef UTILITIES_H
#define UTILITIES_H

char *clean(char *);
int handle_relative_path(char *path, char *cwd, char *results);
char *to_absolute_path(char *, char *);

#endif