#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#define FUSE_USE_VERSION 31

#include <dirent.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "main_helper.h"

// defaults
#define META_INODE_QUANTITY 16
#define META_NODE_QUANTITY 16
#define META_SIZE 2
#define NODE_SIZE 8
#define INODE_FILE_LIM 8
#define INODE_DIR_LIM 8
#define FILE_NAME_LIM 16

typedef struct inode {
  uint8_t flags;
  // uint8_t inode_q;
  // array of offsets to inodes (basically pointers)
  uint32_t inode_off[INODE_DIR_LIM];
  // uint8_t node_q;
  // array of offsets to nodes (basically pointers)
  uint32_t node_off[INODE_FILE_LIM];
  uint32_t parrent;
  char name[FILE_NAME_LIM];
} inode_t;

#define INODE_SIZE sizeof(inode_t)

#define FILE_SIZE                                                              \
  META_INODE_QUANTITY * sizeof(inode_t) + META_NODE_QUANTITY *NODE_SIZE

#define DIR_FLAG 0b10000000
#define FILE_FLAG 0b00000000
#define NODE_USED_FLAG 0b00000001

static int fill_dir_plus = 0;

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

static int r_find_dir_from_inode(int fd, inode_t *inode, char **path,
                                 int path_len, int cur) {

  if (cur == path_len) {
    return 0;
  }

  inode_t *tmp_inode = malloc(INODE_SIZE);
  char *cur_path = path[cur];
  for (int i = 0; i < INODE_DIR_LIM; i++) {
    pread(fd, tmp_inode, INODE_SIZE, inode->inode_off[i]);
    if (strcmp(cur_path, tmp_inode->name) != 0) {
      continue;
    }
    if (path_len == cur + 1) {
      return inode->inode_off[i];
    } else {
      return r_find_dir_from_inode(fd, tmp_inode, path, path_len, cur + 1);
    }
  }
  return -1;
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

  int off = r_find_dir_from_inode(fd, inode, dir_path, n, 0);
  if (off == -1) {
    res = -ENOENT;
  } else {
    pread(fd, inode, INODE_SIZE, off);
    if ((inode->flags & DIR_FLAG) != 0) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    }
  }

  return res;
}
//
// static int xmp_access(const char *path, int mask) {
//   int res;
//
//   res = access(path, mask);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_readlink(const char *path, char *buf, size_t size) {
//   int res;
//
//   res = readlink(path, buf, size - 1);
//   if (res == -1)
//     return -errno;
//
//   buf[res] = '\0';
//   return 0;
// }
//

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

  int off = r_find_dir_from_inode(fd, inode, dir_path, n, 0);
  if (off == -1) {
    close(fd);
    free(inode);
    free(dir_path);
    return -ENOENT;
  }
  pread(fd, inode, INODE_SIZE, off);

  filler(buf, ".", NULL, 0, fill_dir_plus);
  filler(buf, "..", NULL, 0, fill_dir_plus);

  inode_t *tmp = malloc(INODE_SIZE);
  for (int i = 0; i < INODE_DIR_LIM; i++) {
    if (inode->inode_off[i] == 0) {
      continue;
    }
    pread(fd, tmp, INODE_SIZE, inode->inode_off[i]);
    filler(buf, tmp->name, NULL, 0, fill_dir_plus);
  }

  close(fd);
  free(inode);
  free(tmp);
  free(dir_path);
  return 0;
}

// static int r_mknod(const char *path, mode_t mode, dev_t rdev) {
// int res;
//
// res = mknod_wrapper(AT_FDCWD, path, NULL, mode, rdev);
// if (res == -1)
//   return -errno;
//
//   return 0;
// }

static int create_new_dir(char *name, int fd) {

  inode_t *new_inode = malloc(INODE_SIZE);
  memset(new_inode, '\0', INODE_SIZE);
  int name_len = strlen(name);
  for (int i = 0; i < FILE_NAME_LIM && i < name_len; i++) {
    new_inode->name[i] = name[i];
  }
  new_inode->flags = DIR_FLAG | NODE_USED_FLAG;

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

static int r_mkdir(const char *path, mode_t mode) {
  (void)mode;

  int n;
  char **dir_path = get_dirs_from_path(path, &n);

  inode_t *inode = malloc(INODE_SIZE);

  int fd = open(_path, O_RDWR, 0777);
  read(fd, inode, INODE_SIZE);

  // find second to last dir in the path
  int offset = r_find_dir_from_inode(fd, inode, dir_path, n - 1, 0);
  if (offset == -1) {
    return -ENFILE;
  }

  // read inode of second to last dir
  pread(fd, inode, INODE_SIZE, offset);

  // check if it has enough space for new dir child
  int i;
  for (i = 0; i < INODE_DIR_LIM; i++) {
    if (inode->inode_off[i] == 0) {
      break;
    }
  }

  if (i == INODE_DIR_LIM) {
    return -ENFILE;
  }

  int child_offset = create_new_dir(dir_path[n - 1], fd);
  if (offset == -1) {
    return -ENFILE;
  }
  inode->inode_off[i] = child_offset;

  pwrite(fd, inode, INODE_SIZE, offset);

  close(fd);
  free(dir_path);
  return 0;
}

// static int xmp_unlink(const char *path) {
//   int res;
//
//   res = unlink(path);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_rmdir(const char *path) {
//   int res;
//
//   res = rmdir(path);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_symlink(const char *from, const char *to) {
//   int res;
//
//   res = symlink(from, to);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_rename(const char *from, const char *to, unsigned int flags) {
//   int res;
//
//   if (flags)
//     return -EINVAL;
//
//   res = rename(from, to);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_link(const char *from, const char *to) {
//   int res;
//
//   res = link(from, to);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
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
//
// static int xmp_truncate(const char *path, off_t size,
//                         struct fuse_file_info *fi) {
//   int res;
//
//   if (fi != NULL)
//     res = ftruncate(fi->fh, size);
//   else
//     res = truncate(path, size);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// #ifdef HAVE_UTIMENSAT
// static int xmp_utimens(const char *path, const struct timespec ts[2],
//                        struct fuse_file_info *fi) {
//   (void)fi;
//   int res;
//
//   /* don't use utime/utimes since they follow symlinks */
//   res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
// #endif
//
// static int xmp_create(const char *path, mode_t mode,
//                       struct fuse_file_info *fi) {
//   int res;
//
//   res = open(path, fi->flags, mode);
//   if (res == -1)
//     return -errno;
//
//   fi->fh = res;
//   return 0;
// }
//
// static int xmp_open(const char *path, struct fuse_file_info *fi) {
//   int res;
//
//   res = open(path, fi->flags);
//   if (res == -1)
//     return -errno;
//
//   /* Enable direct_io when open has flags O_DIRECT to enjoy the feature
//   parallel_direct_writes (i.e., to get a shared lock, not exclusive lock,
//   for writes to the same file). */
//   if (fi->flags & O_DIRECT) {
//     fi->direct_io = 1;
//     // fi->parallel_direct_writes = 1;
//   }
//
//   fi->fh = res;
//   return 0;
// }
//
// static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
//                     struct fuse_file_info *fi) {
//   int fd;
//   int res;
//
//   if (fi == NULL)
//     fd = open(path, O_RDONLY);
//   else
//     fd = fi->fh;
//
//   if (fd == -1)
//     return -errno;
//
//   res = pread(fd, buf, size, offset);
//   if (res == -1)
//     res = -errno;
//
//   if (fi == NULL)
//     close(fd);
//   return res;
// }
//
// static int xmp_write(const char *path, const char *buf, size_t size,
//                      off_t offset, struct fuse_file_info *fi) {
//   int fd;
//   int res;
//
//   (void)fi;
//   if (fi == NULL)
//     fd = open(path, O_WRONLY);
//   else
//     fd = fi->fh;
//
//   if (fd == -1)
//     return -errno;
//
//   res = pwrite(fd, buf, size, offset);
//   if (res == -1)
//     res = -errno;
//
//   if (fi == NULL)
//     close(fd);
//   return res;
// }
//
// static int xmp_statfs(const char *path, struct statvfs *stbuf) {
//   int res;
//
//   res = statvfs(path, stbuf);
//   if (res == -1)
//     return -errno;
//
//   return 0;
// }
//
// static int xmp_release(const char *path, struct fuse_file_info *fi) {
//   (void)path;
//   close(fi->fh);
//   return 0;
// }
//
// static int xmp_fsync(const char *path, int isdatasync,
//                      struct fuse_file_info *fi) {
//   /* Just a stub.	 This method is optional and can safely be left
//      unimplemented */
//
//   (void)path;
//   (void)isdatasync;
//   (void)fi;
//   return 0;
// }
//
// #ifdef HAVE_POSIX_FALLOCATE
// static int xmp_fallocate(const char *path, int mode, off_t offset, off_t
// length,
//                          struct fuse_file_info *fi) {
//   int fd;
//   int res;
//
//   (void)fi;
//
//   if (mode)
//     return -EOPNOTSUPP;
//
//   if (fi == NULL)
//     fd = open(path, O_WRONLY);
//   else
//     fd = fi->fh;
//
//   if (fd == -1)
//     return -errno;
//
//   res = -posix_fallocate(fd, offset, length);
//
//   if (fi == NULL)
//     close(fd);
//   return res;
// }
// #endif
//
// #ifdef HAVE_SETXATTR
// /* xattr operations are optional and can safely be left unimplemented */
// static int xmp_setxattr(const char *path, const char *name, const char
// *value,
//                         size_t size, int flags) {
//   int res = lsetxattr(path, name, value, size, flags);
//   if (res == -1)
//     return -errno;
//   return 0;
// }
//
// static int xmp_getxattr(const char *path, const char *name, char *value,
//                         size_t size) {
//   int res = lgetxattr(path, name, value, size);
//   if (res == -1)
//     return -errno;
//   return res;
// }
//
// static int xmp_listxattr(const char *path, char *list, size_t size) {
//   int res = llistxattr(path, list, size);
//   if (res == -1)
//     return -errno;
//   return res;
// }
//
// static int xmp_removexattr(const char *path, const char *name) {
//   int res = lremovexattr(path, name);
//   if (res == -1)
//     return -errno;
//   return 0;
// }
// #endif /* HAVE_SETXATTR */
//
// #ifdef HAVE_COPY_FILE_RANGE
// static ssize_t xmp_copy_file_range(const char *path_in,
//                                    struct fuse_file_info *fi_in,
//                                    off_t offset_in, const char *path_out,
//                                    struct fuse_file_info *fi_out,
//                                    off_t offset_out, size_t len, int flags) {
//   int fd_in, fd_out;
//   ssize_t res;
//
//   if (fi_in == NULL)
//     fd_in = open(path_in, O_RDONLY);
//   else
//     fd_in = fi_in->fh;
//
//   if (fd_in == -1)
//     return -errno;
//
//   if (fi_out == NULL)
//     fd_out = open(path_out, O_WRONLY);
//   else
//     fd_out = fi_out->fh;
//
//   if (fd_out == -1) {
//     close(fd_in);
//     return -errno;
//   }
//
//   res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len, flags);
//   if (res == -1)
//     res = -errno;
//
//   if (fi_out == NULL)
//     close(fd_out);
//   if (fi_in == NULL)
//     close(fd_in);
//
//   return res;
// }
// #endif
//
// static off_t xmp_lseek(const char *path, off_t off, int whence,
//                        struct fuse_file_info *fi) {
//   int fd;
//   off_t res;
//
//   if (fi == NULL)
//     fd = open(path, O_RDONLY);
//   else
//     fd = fi->fh;
//
//   if (fd == -1)
//     return -errno;
//
//   res = lseek(fd, off, whence);
//   if (res == -1)
//     res = -errno;
//
//   if (fi == NULL)
//     close(fd);
//   return res;
// }

static const struct fuse_operations r_oper = {
    .init = r_init,
    .getattr = r_getattr,
    // .access = xmp_access,
    // .readlink = xmp_readlink,
    .readdir = r_readdir,
    // .mknod = r_mknod,
    .mkdir = r_mkdir,
    // .symlink = xmp_symlink,
    // .unlink = xmp_unlink,
    // .rmdir = xmp_rmdir,
    // .rename = xmp_rename,
    // .link = xmp_link,
    // .chmod = xmp_chmod,
    // .chown = xmp_chown,
    // .truncate = xmp_truncate,
    // .open = xmp_open,
    // .create = xmp_create,
    // .read = xmp_read,
    // .write = xmp_write,
    // .statfs = xmp_statfs,
    // .release = xmp_release,
    // .fsync = xmp_fsync,
    // .lseek = xmp_lseek,
};

char *concat(const char *s1, const char *s2) {
  char *result = malloc(strlen(s1) + strlen(s2) + 1);
  strcpy(result, s1);
  strcat(result, s2);
  return result;
}

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
