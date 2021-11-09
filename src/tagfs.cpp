/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. This implementation is a little more sophisticated
 * than the one in passthrough.c, so performance is not quite as bad.
 *
 * Compile with:
 *
 *     gcc -Wall passthrough_fh.c `pkg-config fuse3 --cflags --libs` -lulockmgr -o passthrough_fh
 *
 * ## Source code ##
 * \include passthrough_fh.c
 */

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fuse.h>

#ifdef HAVE_LIBULOCKMGR
#include <ulockmgr.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <sys/file.h> /* flock(2) */

#include <TagFS.h>
TagFS tagFS;

struct xmp_dirp {
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res;

    tagvec tag_vec = tagFS.parse_tags(path);
    std::string file_path = tagFS.get_file_real_path(tag_vec);
    res = lstat(file_path.c_str(), stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_access(const char *path, int mask) {
    int res;

    tagvec tag_vec = tagFS.parse_tags(path);
    std::string file_path = tagFS.get_file_real_path(tag_vec);
    res = access(file_path.c_str(), mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
    int res;

    tagvec tag_vec = tagFS.parse_tags(path);
    std::string file_path = tagFS.get_file_real_path(tag_vec);
    res = readlink(file_path.c_str(), buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

// TODO: change inner logic since we have only root dir
static int xmp_opendir(const char *path, struct fuse_file_info *fi) {
    int res;
    auto *d = static_cast<xmp_dirp *>(malloc(sizeof(struct xmp_dirp)));
    if (d == nullptr)
        return -ENOMEM;

    d->dp = opendir(path);
    if (d->dp == nullptr) {
        res = -errno;
        free(d);
        return res;
    }
    d->offset = 0;
    d->entry = nullptr;

    fi->fh = (unsigned long) d;
    return 0;
}

static inline struct xmp_dirp *get_dirp(struct fuse_file_info *fi) {
    return (struct xmp_dirp *) (uintptr_t) fi->fh;
}



// TODO: change inner logic since we have only root dir
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
//    tagvec tags = tagFS.parse_tags(path);
//    inodeset inodes;
//    for (const auto &tag: tags) {
//        auto inode = tagFS.tagInodeMap[tag];
//        inodes.insert(tagFS.tagInodeMap[tag].begin(), tagFS.tagInodeMap[tag].end());
//    }
//
//    for (const auto &inode: inodes) {
//        struct stat st{};
//        lstat(std::to_string(inode).c_str(), &st);      // TODO: error handling
//        filler(buf, tagFS.inodeFilenameMap[inode].c_str(), &st, 0);
//    }




    struct xmp_dirp *d = get_dirp(fi);

    (void) path;
    if (offset != d->offset) {
#ifndef __FreeBSD__
        seekdir(d->dp, offset);
#else
        /* Subtract the one that we add when calling
           telldir() below */
        seekdir(d->dp, offset-1);
#endif
        d->entry = nullptr;
        d->offset = offset;
    }
    while (true) {
        struct stat st{};
        off_t nextoff;
        if (!d->entry) {
            d->entry = readdir(d->dp);
            if (!d->entry)
                break;
        }
#ifdef HAVE_FSTATAT
        if (flags & FUSE_READDIR_PLUS) {
            int res;

            res = fstatat(dirfd(d->dp), d->entry->d_name, &st,
                      AT_SYMLINK_NOFOLLOW);
            if (res != -1)
                fill_flags |= FUSE_FILL_DIR_PLUS;
        }
#endif
//        if (!(fill_flags & FUSE_FILL_DIR_PLUS)) {
//            memset(&st, 0, sizeof(st));
//            st.st_ino = d->entry->d_ino;
//            st.st_mode = d->entry->d_type << 12;
//        }

        memset(&st, 0, sizeof(st));
        st.st_ino = d->entry->d_ino;
        st.st_mode = d->entry->d_type << 12;

        nextoff = telldir(d->dp);
#ifdef __FreeBSD__
        /* Under FreeBSD, telldir() may return 0 the first time
           it is called. But for libfuse, an offset of zero
           means that offsets are not supported, so we shift
           everything by one. */
        nextoff++;
#endif
        if (filler(buf, d->entry->d_name, &st, nextoff))
            break;

        d->entry = nullptr;
        d->offset = nextoff;
    }

    return 0;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi) {
    struct xmp_dirp *d = get_dirp(fi);
    (void) path;
    closedir(d->dp);
    free(d);
    return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
    int res;

    tagvec tag_vec = tagFS.parse_tags(path);
    inodeset file_inode_set = tagFS.get_tag_set(tag_vec);

    if (!file_inode_set.empty()) {
        errno = EEXIST;
#ifdef DEBUG
        std::cerr << "path already exist: " << path << std::endl;
#endif
        return -errno;
    }

    if (S_ISDIR(mode)) {
        // TODO: add additional logic when dir is created (replace with tag creation)
    }
    else {
        inode new_inode = tagFS.get_new_inode();
        std::string new_path = std::to_string(new_inode);

        if (S_ISFIFO(mode)) {
            res = mkfifo(new_path.c_str(), mode);
        }
        else
            res = mknod(new_path.c_str(), mode, rdev);
        if (res == -1)
            return -errno;

        
    }


    return 0;
}

// TODO: handle path correctly and add additional logic when dir is created (replace with tag creation)
static int xmp_mkdir(const char *path, mode_t mode) {
    int res;

    res = mkdir(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}


// TODO: handle path correctly
static int xmp_unlink(const char *path) {
    int res;

    res = unlink(path);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly and add additional logic when dir is deleted (replace with tag deletion)
// Update inner structures
static int xmp_rmdir(const char *path) {
    int res;

    res = rmdir(path);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
static int xmp_symlink(const char *from, const char *to) {
    int res;

    res = symlink(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
// And replace dir rename with tag rename
static int xmp_rename(const char *from, const char *to) {
    int res;

    /* When we have renameat2() in libc, then we can implement flags */

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
static int xmp_link(const char *from, const char *to) {
    int res;

    res = link(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
static int xmp_chmod(const char *path, mode_t mode) {
    int res;
    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
static int xmp_chown(const char *path, uid_t uid, gid_t gid) {
    int res;
    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
static int xmp_truncate(const char *path, off_t size) {
    int res;
    res = truncate(path, size);

    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
               struct fuse_file_info *fi)
{
    int res;

    /* don't use utime/utimes since they follow symlinks */
    if (fi)
        res = futimens(fi->fh, ts);
    else
        res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        return -errno;

    return 0;
}
#endif

// TODO: handle path correctly
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int fd;

    fd = open(path, fi->flags, mode);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

// TODO: handle path correctly
static int xmp_open(const char *path, struct fuse_file_info *fi) {
    int fd;

    fd = open(path, fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}


// TODO: handle path correctly
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}


// TODO: handle path correctly
static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
                        size_t size, off_t offset, struct fuse_file_info *fi) {
    struct fuse_bufvec *src;

    (void) path;

    src = static_cast<fuse_bufvec *>(malloc(sizeof(struct fuse_bufvec)));
    if (src == nullptr)
        return -ENOMEM;

    *src = FUSE_BUFVEC_INIT(size);

    src->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    src->buf[0].fd = fi->fh;
    src->buf[0].pos = offset;

    *bufp = src;

    return 0;
}

// TODO: handle path correctly
static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}


// TODO: handle path correctly
static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
                         off_t offset, struct fuse_file_info *fi) {
    struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    (void) path;

    dst.buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    dst.buf[0].fd = fi->fh;
    dst.buf[0].pos = offset;

    return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

// TODO: handle path correctly
static int xmp_statfs(const char *path, struct statvfs *stbuf) {
    int res;

    res = statvfs(path, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

// TODO: handle path correctly
static int xmp_flush(const char *path, struct fuse_file_info *fi) {
    int res;

    (void) path;
    /* This is called from every close on an open file, so call the
       close on the underlying filesystem.	But since flush may be
       called multiple times for an open file, this must not really
       close the file.  This is important if used on a network
       filesystem like NFS which flush the data/metadata on close() */
    res = close(dup(fi->fh));
    if (res == -1)
        return -errno;

    return 0;
}


// TODO: handle path correctly
static int xmp_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    close(fi->fh);

    return 0;
}


// TODO: handle path correctly
static int xmp_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi) {
    int res;
    (void) path;

#ifndef HAVE_FDATASYNC
    (void) isdatasync;
#else
    if (isdatasync)
        res = fdatasync(fi->fh);
    else
#endif
    res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
            off_t offset, off_t length, struct fuse_file_info *fi)
{
    (void) path;

    if (mode)
        return -EOPNOTSUPP;

    return -posix_fallocate(fi->fh, offset, length);
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
            size_t size, int flags)
{
    int res = lsetxattr(path, name, value, size, flags);
    if (res == -1)
        return -errno;
    return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
            size_t size)
{
    int res = lgetxattr(path, name, value, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
    int res = llistxattr(path, list, size);
    if (res == -1)
        return -errno;
    return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
    int res = lremovexattr(path, name);
    if (res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */

#ifdef HAVE_LIBULOCKMGR
static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
            struct flock *lock)
{
    (void) path;

    return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
               sizeof(fi->lock_owner));
}
#endif
// TODO: handle path correctly
static int xmp_flock(const char *path, struct fuse_file_info *fi, int op) {
    int res;
    (void) path;

    res = flock(fi->fh, op);
    if (res == -1)
        return -errno;

    return 0;
}

#ifdef HAVE_COPY_FILE_RANGE
static ssize_t xmp_copy_file_range(const char *path_in,
                   struct fuse_file_info *fi_in,
                   off_t off_in, const char *path_out,
                   struct fuse_file_info *fi_out,
                   off_t off_out, size_t len, int flags)
{
    ssize_t res;
    (void) path_in;
    (void) path_out;

    res = copy_file_range(fi_in->fh, &off_in, fi_out->fh, &off_out, len,
                  flags);
    if (res == -1)
        return -errno;

    return res;
}
#endif

static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi) {
    off_t res;
    (void) path;

    res = lseek(fi->fh, off, whence);
    if (res == -1)
        return -errno;

    return res;
}

void *xmp_init(struct fuse_conn_info *conn) {
#ifdef __APPLE__
    FUSE_ENABLE_SETVOLNAME(conn);
    FUSE_ENABLE_XTIMES(conn);
#endif
    return nullptr;
}

void
xmp_destroy(void *userdata) {}

static struct fuse_operations xmp_oper = {
        .getattr    = xmp_getattr,
        .readlink   = xmp_readlink,
        .mknod      = xmp_mknod,
        .mkdir      = xmp_mkdir,
        .unlink     = xmp_unlink,
        .rmdir      = xmp_rmdir,
        .symlink    = xmp_symlink,
        .rename     = xmp_rename,
        .link       = xmp_link,
        .chmod      = xmp_chmod,
        .chown      = xmp_chown,
        .truncate   = xmp_truncate,
        .open       = xmp_open,
        .read       = xmp_read,
        .write      = xmp_write,
        .statfs     = xmp_statfs,
        .flush      = xmp_flush,
        .release    = xmp_release,
        .fsync      = xmp_fsync,
        .opendir    = xmp_opendir,
        .readdir    = xmp_readdir,
        .releasedir = xmp_releasedir,
        .init       = xmp_init,
        .destroy	= xmp_destroy,
        .access     = xmp_access,

#ifdef HAVE_UTIMENSAT
        .utimens	= xmp_utimens,
#endif
        .create        = xmp_create,
        .write_buf    = xmp_write_buf,
        .read_buf    = xmp_read_buf,
#ifdef HAVE_POSIX_FALLOCATE
        .fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
        .setxattr	= xmp_setxattr,
    .getxattr	= xmp_getxattr,
    .listxattr	= xmp_listxattr,
    .removexattr	= xmp_removexattr,
#endif
#ifdef HAVE_LIBULOCKMGR
        .lock		= xmp_lock,
#endif
        .flock        = xmp_flock
#ifdef HAVE_COPY_FILE_RANGE
        .copy_file_range = xmp_copy_file_range,
#endif
};

void init_from_dir(const std::string &root){
    std::size_t inode_count = 0;
    for(auto& p: fs::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)){
        tag_t tag;
        tag.id = inode_count;
        tag.name = *(--p.path().end());
        if(!std::filesystem::is_directory(p)){
            tag.type = TAG_NAME;
        } else {
            tag.type = FILE_NAME;
            tagFS.inodeFilenameMap.insert({inode_count, tag.name});
        }
        tagFS.inodeTagMap[inode_count].insert(tag);
        tagFS.tagInodeMap[tag].insert(inode_count);
        tagFS.tagNameTag.insert({tag.name, tag});
        inode_count++;
    }

    std::cout << inode_count << std::endl;
    std::cout << tagFS.inodeTagMap.size() << std::endl;
    std::cout << tagFS.tagInodeMap.size() << std::endl;
    std::cout << tagFS.tagNameTag.size() << std::endl;
    std::cout << tagFS.inodeFilenameMap.size() << std::endl;
}

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, nullptr);

}

