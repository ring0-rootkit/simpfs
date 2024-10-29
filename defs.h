#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>
#define META_INODE_QUANTITY 16
#define META_NODE_QUANTITY 32
#define META_SIZE 2
#define PAGE_SIZE 1024
#define INODE_INODE_LIM 8
#define FILE_NAME_LIM 64

typedef struct inode {
  uint8_t flags;
  // uint8_t inode_q;
  // array of offsets to inodes (basically pointers)
  uint32_t inode_off[INODE_INODE_LIM];
  // uint8_t node_q;
  // array of offsets to nodes (basically pointers)
  uint32_t node_off;
  uint32_t parrent;
  char name[FILE_NAME_LIM + 1];
} inode_t;

typedef struct node {
  uint8_t flags;
  uint32_t next;
  uint32_t prev;
  uint32_t len;
  char data[PAGE_SIZE + 1];
} node_t;

#define INODE_SIZE sizeof(inode_t)
#define NODE_SIZE sizeof(node_t)

#define FILE_SIZE                                                              \
  (META_INODE_QUANTITY * INODE_SIZE) + (META_NODE_QUANTITY * NODE_SIZE)

#define DIR_FLAG 0b10000000
#define FILE_FLAG 0b00000000
#define NODE_USED_FLAG 0b00000001

#endif
