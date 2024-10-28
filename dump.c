#include "defs.h"
#include "main_helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int SHOW_NODE_DATA = 0;

void print_inode(inode_t *inode, int offset) {
  if ((inode->flags & NODE_USED_FLAG) == 0) {
    return;
  }

  char *type = "dir";
  if ((inode->flags & DIR_FLAG) == 0) {
    type = "file";
  }
  printf("\tinode(%s): %s\n", type, inode->name);
  printf("id:%d\n", offset);
  printf("parent:%d\n", inode->parrent);
  printf("inodes:");
  for (int i = 0; i < INODE_INODE_LIM; i++) {
    int id = inode->inode_off[i];
    if (id == 0)
      continue;
    printf(" %d ", id);
  }
  printf("\n");
  printf("nodes:");
  for (int i = 0; i < INODE_NODE_LIM; i++) {
    int id = inode->node_off[i];
    if (id == 0)
      continue;
    printf(" %d ", id);
  }
  printf("\n");
  printf("\n\n");
}
void print_node(node_t *node, int offset) {
  if ((node->flags & NODE_USED_FLAG) == 0) {
    return;
  }

  printf("\tnode:\n");
  printf("id:%d\n", offset);
  printf("parent:%d\n", node->parent);
  printf("data (%ld bytes):\n", strlen(node->data));
  if (SHOW_NODE_DATA) {
    printf("===[START]===\n");
    printf("%s\n", node->data);
    printf("===[END]===\n");
  }
  printf("\n\n");
}

int main(int argc, char **argv) {

  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0) {
      SHOW_NODE_DATA = 1;
    }
  }

  char *rel_path = realpath("./", NULL);
  char *_path = concat(rel_path, "/data");
  if (_path == NULL) {
    printf("cannot find file with name[%s]\n", rel_path);
    exit(1);
  }

  int fd = open(_path, O_RDWR | O_CREAT, 0777);
  if (fd == -1) {
    printf("error\n");
    free(_path);
    free(rel_path);
    exit(1);
  }

  inode_t *inode = malloc(INODE_SIZE);
  node_t *node = malloc(NODE_SIZE);

  int offset = 0;
  for (int i = 0; i < META_INODE_QUANTITY; i++) {
    pread(fd, inode, INODE_SIZE, offset);
    print_inode(inode, offset);
    offset += INODE_SIZE;
  }

  for (int i = 0; i < META_NODE_QUANTITY; i++) {
    pread(fd, node, NODE_SIZE, offset);
    print_node(node, offset);
    offset += NODE_SIZE;
  }

  free(_path);
  free(rel_path);
  close(fd);
}
