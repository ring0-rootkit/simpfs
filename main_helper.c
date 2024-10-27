#include "main_helper.h"
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
  char *path_cpy = malloc(len);
  strcpy(path_cpy, path);
  *n = num_of_chars(path_cpy, '/');
  if (path_cpy[len] == '/') {
    *n = *n - 1;
  }
  int i = 0;
  char **ret = malloc(*n);
  char *dir = strtok(path_cpy, "/");
  while (dir) {
    ret[i] = dir;
    i++;
    dir = strtok(NULL, "/");
  }
  return ret;
}
