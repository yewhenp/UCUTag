#ifndef UCUTAG_PROJECT_TAGFS_API_H
#define UCUTAG_PROJECT_TAGFS_API_H

#include <fuse.h>

static int ucutag_getattr(const char *path, struct stat *stbuf);
static int ucutag_access(const char *path, int mask);
static int ucutag_readlink(const char *path, char *buf, size_t size);
static int ucutag_opendir(const char *path, struct fuse_file_info *fi);
static int ucutag_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi);
static int ucutag_releasedir(const char *path, struct fuse_file_info *fi);
static int ucutag_mkdir(const char *path, mode_t mode);
static int ucutag_mknod(const char *path, mode_t mode, dev_t rdev);
static int ucutag_unlink(const char *path);
static int ucutag_rmdir(const char *path);
static int ucutag_symlink(const char *from, const char *to);
static int ucutag_rename(const char *from, const char *to);
static int ucutag_link(const char *from, const char *to);
static int ucutag_chmod(const char *path, mode_t mode);
static int ucutag_chown(const char *path, uid_t uid, gid_t gid);
static int ucutag_truncate(const char *path, off_t size);
static int ucutag_open(const char *path, struct fuse_file_info *fi);
static int ucutag_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);
static int ucutag_read_buf(const char *path, struct fuse_bufvec **bufp,
                        size_t size, off_t offset, struct fuse_file_info *fi);
static int ucutag_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi);
static int ucutag_write_buf(const char *path, struct fuse_bufvec *buf,
                         off_t offset, struct fuse_file_info *fi);
static int ucutag_statfs(const char *path, struct statvfs *stbuf);
static int ucutag_flush(const char *path, struct fuse_file_info *fi);
static int ucutag_release(const char *path, struct fuse_file_info *fi);
static int ucutag_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi);
static int ucutag_flock(const char *path, struct fuse_file_info *fi, int op);
void *ucutag_init(struct fuse_conn_info *conn);
int ucutag_utimens(const char *, const struct timespec tv[2]);
void ucutag_destroy(void *userdata);


#endif //UCUTAG_PROJECT_TAGFS_API_H
