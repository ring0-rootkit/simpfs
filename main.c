/*
 * Implementation of user space filesystem using libfuse
 * files are stored in 'data' file in the same dir as _simpfs_ mount point
 *
 * Concepts:
 *  - inode - information node (stores dir/file metadata)
 *  - node - stores file data
 *
 * */
#include <linux/limits.h>
#define FUSE_USE_VERSION 31

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "main_helper.h"

#include "defs.h"

static int FILL_DIR_PLUS = 0;

static char *_path;

static void *r_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  (void)cfg;
  inode_t *root_inode = malloc(INODE_SIZE);
  memset(root_inode, '\0', INODE_SIZE);

  root_inode->name[0] = '/';
  root_inode->flags = DIR_FLAG | NODE_USED_FLAG;

  int fd = open(_path, O_WRONLY, 0777);
  write(fd, root_inode, INODE_SIZE);
  close(fd);
  return NULL;
}

static int create_new_inode(char *name, int fd, int parrent_offset,
                            uint8_t type_flag) {

  inode_t *new_inode = malloc(INODE_SIZE);
  memset(new_inode, '\0', INODE_SIZE);
  int name_len = strlen(name);
  for (int i = 0; i < FILE_NAME_LIM && i < name_len; i++) {
    new_inode->name[i] = name[i];
  }
  new_inode->flags = type_flag | NODE_USED_FLAG;
  new_inode->parrent = parrent_offset;

  inode_t *tmp_inode = malloc(INODE_SIZE);
  int offset = 0;
  int i;
  for (i = 0; i < META_INODE_QUANTITY; i++) {
    pread(fd, tmp_inode, INODE_SIZE, offset);
    // if not used
    if ((tmp_inode->flags & NODE_USED_FLAG) == 0) {
      break;
    }
    offset += INODE_SIZE;
  }
  if (i == META_INODE_QUANTITY) {
    free(new_inode);
    free(tmp_inode);
    return -1;
  }
  pwrite(fd, new_inode, INODE_SIZE, offset);

  free(new_inode);
  free(tmp_inode);
  return offset;
}

static int create_new_node(int fd, int parrent_offset, const char *buf,
                           int buf_offset, int file_start, int data_size) {

  node_t *new_node = malloc(NODE_SIZE);
  memset(new_node, '\0', NODE_SIZE);
  new_node->flags = NODE_USED_FLAG;
  new_node->parent = parrent_offset;

  inode_t *tmp_node = malloc(NODE_SIZE);
  int offset = META_INODE_QUANTITY * INODE_SIZE;
  int i;
  for (i = 0; i < META_NODE_QUANTITY; i++) {
    pread(fd, tmp_node, NODE_SIZE, offset);
    // if not used
    if ((tmp_node->flags & NODE_USED_FLAG) == 0) {
      break;
    }
    offset += NODE_SIZE;
  }
  if (i == META_NODE_QUANTITY) {
    free(new_node);
    free(tmp_node);
    return -1;
  }

  int len = PAGE_SIZE;
  if (data_size < len) {
    len = data_size;
  }

  for (int i = 0; i < len; i++) {
    new_node->data[file_start + i] = buf[buf_offset + i];
  }

  pwrite(fd, new_node, NODE_SIZE, offset);

  free(new_node);
  free(tmp_node);
  return offset;
}

static int r_find_inode_by_path(int fd, inode_t *inode, char **path,
                                int path_len, int cur) {

  if (cur == path_len) {
    return 0;
  }

  inode_t *tmp_inode = malloc(INODE_SIZE);
  char *cur_path = path[cur];
  for (int i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] == 0) {
      continue;
    }
    pread(fd, tmp_inode, INODE_SIZE, inode->inode_off[i]);
    if (strcmp(cur_path, tmp_inode->name) != 0) {
      continue;
    }
    if (path_len == cur + 1) {
      free(tmp_inode);
      return inode->inode_off[i];
    } else {
      int _r = r_find_inode_by_path(fd, tmp_inode, path, path_len, cur + 1);
      free(tmp_inode);
      return _r;
    }
  }
  free(tmp_inode);
  return -1;
}

static int r_get_file_size(int fd, inode_t *inode) {
  int size = 0;
  node_t *node = malloc(NODE_SIZE);
  for (int i = 0; i < INODE_NODE_LIM; i++) {
    if (inode->node_off[i] == 0) {
      break;
    }
    pread(fd, node, NODE_SIZE, inode->node_off[i]);
    if ((node->flags & NODE_USED_FLAG) == 0) {
      break;
    }
    size += strlen(node->data);
  }
  free(node);
  return size;
}

static int r_getattr(const char *path, struct stat *stbuf,
                     struct fuse_file_info *fi) {
  (void)fi;
  int res = 0;

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return res;
  }

  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  int fd = open(_path, O_RDONLY, 0777);
  inode_t *inode = malloc(INODE_SIZE);
  pread(fd, inode, INODE_SIZE, 0);

  int off = r_find_inode_by_path(fd, inode, dir_path, n, 0);
  if (off == -1) {
    free(dir_path);
    free(inode);
    close(fd);
    return -ENOENT;
  }

  free(dir_path);

  pread(fd, inode, INODE_SIZE, off);

  if ((inode->flags & NODE_USED_FLAG) == 0) {
    free(inode);
    close(fd);
    return -ENOENT;
  }

  if ((inode->flags & DIR_FLAG) != 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else {
    stbuf->st_mode = S_IFREG | 0755;
    stbuf->st_nlink = 1;
    stbuf->st_size = r_get_file_size(fd, inode);
  }

  free(inode);
  close(fd);
  return res;
}

static int r_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                     off_t offset, struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags) {
  (void)offset;
  (void)fi;
  (void)flags;

  if (path[0] != '/')
    return -ENOENT;

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDONLY, 0777);
  read(fd, inode, INODE_SIZE);

  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  int off = r_find_inode_by_path(fd, inode, dir_path, n, 0);
  if (off == -1) {
    close(fd);
    free(inode);
    free(dir_path);
    return -ENOENT;
  }
  pread(fd, inode, INODE_SIZE, off);

  filler(buf, ".", NULL, 0, FILL_DIR_PLUS);
  filler(buf, "..", NULL, 0, FILL_DIR_PLUS);

  inode_t *tmp = malloc(INODE_SIZE);
  for (int i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] == 0) {
      continue;
    }
    pread(fd, tmp, INODE_SIZE, inode->inode_off[i]);
    if ((tmp->flags & NODE_USED_FLAG) == 0) {
      continue;
    }
    filler(buf, tmp->name, NULL, 0, FILL_DIR_PLUS);
  }

  close(fd);
  free(inode);
  free(tmp);
  free(dir_path);
  return 0;
}

static int r_mkdir(const char *path, mode_t mode) {
  (void)mode;

  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDWR, 0777);
  read(fd, inode, INODE_SIZE);

  // find second to last dir in the path
  int offset = r_find_inode_by_path(fd, inode, dir_path, n - 1, 0);
  if (offset == -1) {
    free(inode);
    close(fd);
    free(dir_path);
    return -ENFILE;
  }

  // read inode of second to last dir
  pread(fd, inode, INODE_SIZE, offset);

  // check if it has enough space for new dir child
  int i;
  for (i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] == 0) {
      break;
    }
  }

  if (i == INODE_INODE_LIM) {
    free(inode);
    close(fd);
    free(dir_path);
    return -ENFILE;
  }

  int child_offset = create_new_inode(dir_path[n - 1], fd, offset, DIR_FLAG);
  if (child_offset == -1) {
    free(inode);
    close(fd);
    free(dir_path);
    return -ENFILE;
  }
  inode->inode_off[i] = child_offset;

  pwrite(fd, inode, INODE_SIZE, offset);

  close(fd);
  free(dir_path);
  free(inode);
  return 0;
}

static int r_unlink(const char *path) {
  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  int fd = open(_path, O_RDWR, 0777);

  inode_t *inode = malloc(INODE_SIZE);
  read(fd, inode, INODE_SIZE);
  int child_offset = r_find_inode_by_path(fd, inode, dir_path, n, 0);

  pread(fd, inode, INODE_SIZE, child_offset);

  inode->flags = 0;

  pwrite(fd, inode, INODE_SIZE, child_offset);

  node_t *node = malloc(NODE_SIZE);
  for (int i = 0; i < INODE_NODE_LIM; i++) {
    if (inode->node_off[i] == 0) {
      continue;
    }
    pread(fd, node, NODE_SIZE, inode->node_off[i]);
    node->flags = 0;
    pwrite(fd, node, NODE_SIZE, inode->node_off[i]);
  }

  int parrent = inode->parrent;

  pread(fd, inode, INODE_SIZE, parrent);

  for (int i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] == child_offset) {
      inode->inode_off[i] = 0;
    }
  }

  pwrite(fd, inode, INODE_SIZE, parrent);

  close(fd);
  free(inode);
  free(node);
  free(dir_path);
  return 0;
}

static int r_rmdir(const char *path) {
  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDWR, 0777);
  read(fd, inode, INODE_SIZE);

  // find second to last dir in the path
  int offset = r_find_inode_by_path(fd, inode, dir_path, n, 0);
  if (offset == -1) {
    free(dir_path);
    free(inode);
    close(fd);
    return -ENFILE;
  }
  pread(fd, inode, INODE_SIZE, offset);
  inode->flags = inode->flags & (~NODE_USED_FLAG);
  for (int i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] != 0) {
      free(dir_path);
      free(inode);
      close(fd);
      return -ENOTEMPTY;
    }
  }
  pwrite(fd, inode, INODE_SIZE, offset);

  int parrent_off = inode->parrent;
  pread(fd, inode, INODE_SIZE, parrent_off);
  for (int i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] == offset) {
      inode->inode_off[i] = 0;
    }
  }
  pwrite(fd, inode, INODE_SIZE, parrent_off);

  free(dir_path);
  free(inode);
  close(fd);
  return 0;
}

static int r_rename(const char *from, const char *to, unsigned int flags) {
  int from_n, to_n;
  char **dir_path_from = get_dirs_from_path(from, &from_n);
  char **dir_path_to = get_dirs_from_path(to, &to_n);
  char *new_name = dir_path_to[to_n - 1];

  int fd = open(_path, O_RDWR, 0777);
  inode_t *inode = malloc(INODE_SIZE);
  read(fd, inode, INODE_SIZE);

  int offset = r_find_inode_by_path(fd, inode, dir_path_from, from_n, 0);
  pread(fd, inode, INODE_SIZE, offset);
  for (int i = 0; i < NAME_MAX && i < strlen(new_name); i++) {
    inode->name[i] = new_name[i];
  }
  pwrite(fd, inode, INODE_SIZE, offset);

  free(dir_path_to);
  free(dir_path_from);
  free(inode);
  close(fd);
  return 0;
}

// static int xmp_chmod(const char *path, mode_t mode, struct fuse_file_info
// *fi) {
//   (void)fi;
//   int res;
//
//   res = chmod(path, mode);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_chown(const char *path, uid_t uid, gid_t gid,
//                      struct fuse_file_info *fi) {
//   (void)fi;
//   int res;
//
//   res = lchown(path, uid, gid);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }

static int r_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  (void)mode;
  (void)fi;

  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDWR, 0777);
  read(fd, inode, INODE_SIZE);

  // find second to last dir in the path
  int offset = r_find_inode_by_path(fd, inode, dir_path, n - 1, 0);
  if (offset == -1) {
    free(inode);
    free(dir_path);
    close(fd);
    return -ENFILE;
  }

  // read inode of second to last dir
  pread(fd, inode, INODE_SIZE, offset);

  // check if it has enough space for new dir child
  int i;
  for (i = 0; i < INODE_INODE_LIM; i++) {
    if (inode->inode_off[i] == 0) {
      break;
    }
  }

  if (i == INODE_INODE_LIM) {
    free(inode);
    free(dir_path);
    close(fd);
    return -ENFILE;
  }

  int child_offset = create_new_inode(dir_path[n - 1], fd, offset, FILE_FLAG);
  if (child_offset == -1) {
    free(inode);
    free(dir_path);
    close(fd);
    return -ENFILE;
  }
  inode->inode_off[i] = child_offset;

  pwrite(fd, inode, INODE_SIZE, offset);

  close(fd);
  free(dir_path);
  free(inode);
  return 0;
}

static int r_open(const char *path, struct fuse_file_info *fi) {
  int n;
  char **dir_path = get_dirs_from_path(path, &n);
  int fd = open(_path, O_RDONLY, 0777);
  inode_t *inode = malloc(INODE_SIZE);
  read(fd, inode, INODE_SIZE);
  int offset = r_find_inode_by_path(fd, inode, dir_path, n, 0);
  if (offset == -1) {
    free(inode);
    free(dir_path);
    close(fd);
    return -ENOENT;
  }

  free(dir_path);
  free(inode);
  close(fd);

  return 0;
}

static int r_read(const char *path, char *buf, size_t size, off_t offset,
                  struct fuse_file_info *fi) {
  fi->direct_io = 1;
  (void)fi;

  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDWR, 0777);
  read(fd, inode, INODE_SIZE);

  // find second to last dir in the path
  int inode_offset = r_find_inode_by_path(fd, inode, dir_path, n, 0);
  if (inode_offset == -1) {
    free(inode);
    free(dir_path);
    close(fd);
    return -ENFILE;
  }

  // read inode of second to last dir
  pread(fd, inode, INODE_SIZE, inode_offset);

  int len = r_get_file_size(fd, inode) - offset;
  if (len > size + offset) {
    len = size;
  }
  if (len == 0) {
    close(fd);
    free(dir_path);
    free(inode);
    return 0;
  }

  node_t *node = malloc(NODE_SIZE);

  int page = (int)(offset / PAGE_SIZE);
  int cur_it = offset - page * PAGE_SIZE;

  pread(fd, node, NODE_SIZE, inode->node_off[page]);
  page++;

  for (int i = 0; i < len; i++) {
    buf[i] = node->data[cur_it];
    cur_it++;
    if (cur_it != PAGE_SIZE) {
      continue;
    }
    if (inode->node_off[page] == 0) {
      break;
    }
    pread(fd, node, NODE_SIZE, inode->node_off[page]);
    if ((node->flags & NODE_USED_FLAG) == 0) {
      break;
    }
    cur_it = 0;
    page++;
  }

  close(fd);
  free(dir_path);
  free(inode);
  free(node);
  return len;
}

static int r_write(const char *path, const char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  fi->direct_io = 1;
  (void)fi;
  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDWR, 0777);
  read(fd, inode, INODE_SIZE);

  int inode_offset = r_find_inode_by_path(fd, inode, dir_path, n, 0);
  if (inode_offset == -1) {
    free(dir_path);
    free(inode);
    close(fd);
    return -ENFILE;
  }

  pread(fd, inode, INODE_SIZE, inode_offset);

  if ((inode->flags & DIR_FLAG) != 0) {
    free(dir_path);
    free(inode);
    close(fd);
    return -EISDIR;
  }

  int page = (int)(offset / PAGE_SIZE);
  int cur_off = offset - (page * PAGE_SIZE);
  int buf_off = 0;
  int n_read = 0;

  node_t *node = malloc(NODE_SIZE);

  while (page < INODE_NODE_LIM && size > 0) {
    int write_len = PAGE_SIZE - cur_off;
    if (write_len > size) {
      write_len = size;
    }
    if (inode->node_off[page] == 0) {
      int child_offset =
          create_new_node(fd, inode_offset, buf, buf_off, 0, write_len);
      if (child_offset == -1) {
        close(fd);
        free(dir_path);
        free(inode);
        free(node);
        return -ENFILE;
      }
      inode->node_off[page] = child_offset;
    } else {
      pread(fd, node, NODE_SIZE, inode->node_off[page]);

      for (int i = 0; i < write_len; i++) {
        node->data[cur_off + i] = buf[buf_off + i];
      }

      pwrite(fd, node, NODE_SIZE, inode->node_off[page]);
    }
    buf_off += write_len;
    cur_off = 0;
    size -= write_len;
    n_read += write_len;
    page++;
  }

  pwrite(fd, inode, INODE_SIZE, inode_offset);

  close(fd);
  free(dir_path);
  free(node);
  free(inode);
  return n_read;
}

static int r_release(const char *path, struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  return 0;
}

static const struct fuse_operations r_oper = {
    .init = r_init,
    .getattr = r_getattr,
    .readdir = r_readdir,
    .mkdir = r_mkdir,
    .unlink = r_unlink,
    .rmdir = r_rmdir,
    .rename = r_rename,
    // .chmod = xmp_chmod,
    // .chown = xmp_chown,
    .open = r_open,
    .create = r_create,
    .read = r_read,
    .write = r_write,
    .release = r_release,
};

int main(int argc, char *argv[]) {
  umask(0);
  char *rel_path = realpath("./", NULL);
  _path = concat(rel_path, "/data");
  if (_path == NULL) {
    printf("cannot find file with name[%s]\n", rel_path);
    exit(1);
  }

  int fd = open(_path, O_RDWR | O_CREAT, 0777);

  if (fd == -1) {
    printf("error\n");
    close(fd);
    free(_path);
    free(rel_path);
    exit(1);
  }
  for (int i = 0; i < FILE_SIZE; i++) {
    int n = write(fd, "\0", 1);
    if (n == -1) {
      printf("error\n");
      close(fd);
      free(_path);
      free(rel_path);
      exit(1);
    }
  }
  close(fd);

  int ret = fuse_main(argc, argv, &r_oper, NULL);

  free(_path);
  free(rel_path);

  return ret;
}
