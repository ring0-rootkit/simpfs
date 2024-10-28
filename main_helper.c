#include "main_helper.h"
#include "defs.h"
#include <stdlib.h>
#include <string.h>
int num_of_chars(const char *str, const char c) {
  int counter = 0;
  int len = strlen(str);
  for (int i = 0; i < len - 1; i++) {
    if (str[i] == c) {
      counter++;
    }
  }
  return counter;
}

char **get_dirs_from_path(const char *path, int *n) {
  *n = 0;
  if (strcmp(path, "/") == 0 || strlen(path) == 0) {
    return NULL;
  }
  int len = strlen(path);
  char path_cpy[len + 1];
  strcpy(path_cpy, path);
  *n = num_of_chars(path_cpy, '/');
  if (path_cpy[len - 1] == '/') {
    *n = *n - 1;
  }

  int i = 0;
  char **ret = malloc(*n);
  char *dir = strtok(path_cpy, "/");
  while (dir && i < *n) {
    int dir_len = strlen(dir);
    ret[i] = malloc(dir_len + 1);
    memcpy(ret[i], dir, dir_len);
    ret[i][dir_len] = '\0';
    i++;
    dir = strtok(NULL, "/");
  }
  return ret;
}

char *concat(const char *s1, const char *s2) {
  char *result = malloc(strlen(s1) + strlen(s2) + 1);
  strcpy(result, s1);
  strcat(result, s2);
  return result;
}
