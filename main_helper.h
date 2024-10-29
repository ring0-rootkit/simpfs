
#ifndef main_helper_h
#define main_helper_h

#include <stdlib.h>
#include <string.h>

int num_of_chars(const char *str, const char c);
char **get_dirs_from_path(const char *path, int *n);

char *concat(const char *s1, const char *s2);
#endif
