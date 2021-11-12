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

struct tag_dirp {
    inodeset inodes;
    inodeset::iterator entry;
};

static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res;

    if ( strcmp( path, "/" ) == 0 ) {
        stbuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
        stbuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
        stbuf->st_atime = time( nullptr ); // The last "a"ccess of the file/directory is right now
        stbuf->st_mtime = time( nullptr ); // The last "m"odification of the file/directory is right now
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is here: http://unix.stackexchange.com/a/101536
        return 0;
    }
    auto tags_status = tagFS.parseTags(path);
    if (tags_status.second != 0) return -errno;
    tagvec tag_vec = tags_status.first;
    std::string file_path = tagFS.getFileRealPath(tag_vec);
    res = lstat(file_path.c_str(), stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_access(const char *path, int mask) {
    int res;

    auto tags_status = tagFS.parseTags(path);
    if (tags_status.second != 0) return -errno;
    tagvec tag_vec = tags_status.first;
    std::string file_path = tagFS.getFileRealPath(tag_vec);
    res = access(file_path.c_str(), mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
    int res;

    auto tags_status = tagFS.parseTags(path);
    if (tags_status.second != 0) return -errno;
    tagvec tag_vec = tags_status.first;
    std::string file_path = tagFS.getFileRealPath(tag_vec);
    res = readlink(file_path.c_str(), buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int xmp_opendir(const char *path, struct fuse_file_info *fi) {

    int res = 0;

    // create dirent
    auto *d = static_cast<tag_dirp *> (malloc(sizeof(struct tag_dirp)));
    if (d == nullptr)
        return -ENOMEM;

    // fill and save pointer to dirent
    auto tags_status = tagFS.parseTags(path);
    if (tags_status.second != 0) return -errno;
    tagvec tags = tags_status.first;
    d->inodes = std::move(tagFS.getInodesFromTags(tags));
    d->entry = d->inodes.begin();
    fi->fh = (unsigned long) d;

    return res;
}

static inline struct tag_dirp *get_dirp(struct fuse_file_info *fi) {
    return (struct tag_dirp *) (uintptr_t) fi->fh;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    int status = 0;
    tag_dirp *d = get_dirp(fi);

    // get first entry
    d->entry = d->inodes.begin();

    // move entry on offset
    std::advance(d->entry, offset);

    while (d->entry != d->inodes.end()) {
        struct stat st{};
        if (lstat(std::to_string(*d->entry).c_str(), &st) == -1) status = -1;
        filler(buf, tagFS.inodeFilenameMap[*d->entry].c_str(), &st, 0);
    }

    return status;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi) {
    tag_dirp *d = get_dirp(fi);
    free(d);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
//    auto spath = std::string(path);
    strvec tagNames = split(path, "/");
    return tagFS.createRegularTags(tagNames);
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
    int res;

    auto tags_status = tagFS.parseTags(path);  // will fail, since tags are not present
    if (tags_status.second != 0) return -errno;
    tagvec tag_vec = tags_status.first;
    inodeset file_inode_set = tagFS.getInodesFromTags(tag_vec);

    if (!file_inode_set.empty()) {
        errno = EEXIST;
#ifdef DEBUG
        std::cerr << "path already exist: " << path << std::endl;
#endif
        return -errno;
    }

    if (S_ISDIR(mode)) {
        xmp_mkdir(path, mode);
    }
    else {
        inode new_inode = tagFS.getNewInode();
        std::string new_path = std::to_string(new_inode);

        if (S_ISFIFO(mode)) {
            res = mkfifo(new_path.c_str(), mode);
        }
        else {
            res = mknod(new_path.c_str(), mode, rdev);
        }
        if (res == -1) {
            return -errno;
        }

        if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
            unlink(new_path.c_str());
            errno = EEXIST;
            return -errno;
        }

    }

    return 0;
}

static int xmp_unlink(const char *path) {
    int res;
    auto tags_status = tagFS.parseTags(path);
    if (tags_status.second != 0) return -errno;
    tagvec tag_vec = tags_status.first;
    std::string file_path = tagFS.getFileRealPath(tag_vec);
    inode file_inode = tagFS.getFileInode(tag_vec);

    if (tagFS.deleteFileMetaData(tag_vec, file_inode) != 0) {
        errno = ENOENT;
        return -errno;
    }

    res = unlink(file_path.c_str());
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_rmdir(const char *path) {
//    auto spath = std::string(path);
    strvec tagNames = split(path, "/");
    return tagFS.deleteRegularTags(tagNames);
}

static int xmp_symlink(const char *from, const char *to) {
    int res;

    auto tags_status_to = tagFS.parseTags(from);
    if (tags_status_to.second != 0) return -errno;
    tagvec tag_vec_to = tags_status_to.first;
    inodeset file_inode_set = tagFS.getInodesFromTags(tag_vec_to);

    auto tags_status_from = tagFS.parseTags(from);
    if (tags_status_from.second != 0) return -errno;
    tagvec tag_vec_from = tags_status_from.first;
    std::string link_file_path = tagFS.getFileRealPath(tag_vec_from);

    inode new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);

    res = symlink(link_file_path.c_str(), new_path.c_str());
    if (res == -1)
        return -errno;

    if (tagFS.createNewFileMetaData(tag_vec_to, new_inode) != 0) {
        unlink(new_path.c_str());
        errno = EEXIST;
        return -errno;
    }

    return 0;
}

// TODO: Replace dir rename with tag rename
static int xmp_rename(const char *from, const char *to) {
    auto[tag_vec_from, status] = tagFS.parseTags(from);
    if (status != 0) return -errno;

    auto tag_vec_to = tagvec();
    for(auto& name: split(to, "/")){
        tag_vec_to.push_back({TAG_TYPE_REGULAR, name});
    }
    tag_vec_to.back().type = tag_vec_from.back().type;

    if (tag_vec_from[tag_vec_from.size() - 1].type == TAG_TYPE_FILE) {
        inode file_inode = tagFS.getFileInode(tag_vec_from);

        if (tagFS.deleteFileMetaData(tag_vec_from, file_inode) != 0) {
            errno = ENOENT;
            return -errno;
        }
        if (tagFS.createNewFileMetaData(tag_vec_to, file_inode) != 0) {
            errno = EEXIST;
            return -errno;
        }
    } else {
//        /tag1/tag2/  ->  /tag1_new/tag2_new
        if (tag_vec_from.size() != tag_vec_to.size()) {
#ifdef DEBUG
            std::cerr << "When renaming multiple tags, froms and tos should be of the same length" << std::endl;
#endif
            errno = EINVAL;
            return -errno;
        }


    }

    return 0;
}

static int xmp_link(const char *from, const char *to) {
    int res;

    auto[tag_vec_to, status] = tagFS.parseTags(to);
    if (status != 0) return -errno;
    inodeset file_inode_set = tagFS.getInodesFromTags(tag_vec_to);

    auto[tag_vec_from, status1] = tagFS.parseTags(from);
    if (status1 != 0) return -errno;
    std::string link_file_path = tagFS.getFileRealPath(tag_vec_from);

    inode new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);

    res = link(link_file_path.c_str(), new_path.c_str());
    if (res == -1)
        return -errno;

    if (tagFS.createNewFileMetaData(tag_vec_to, new_inode) != 0) {
        unlink(new_path.c_str());
        errno = EEXIST;
        return -errno;
    }

    return 0;
}

static int xmp_chmod(const char *path, mode_t mode) {
    int res;
    auto[tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tag_vec);

    res = chmod(file_path.c_str(), mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid) {
    int res;
    auto[tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tag_vec);

    res = lchown(file_path.c_str(), uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_truncate(const char *path, off_t size) {
    int res;
    auto[tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tag_vec);

    res = truncate(file_path.c_str(), size);

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
    auto[tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    inodeset file_inode_set = tagFS.getInodesFromTags(tag_vec);

    if (!file_inode_set.empty()) {
        errno = EEXIST;
#ifdef DEBUG
        std::cerr << "path already exist: " << path << std::endl;
#endif
        return -errno;
    }

    inode new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);

    fd = open(new_path.c_str(), fi->flags, mode);
    if (fd == -1)
        return -errno;


    if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
        close(fd);
        unlink(new_path.c_str());
        errno = EEXIST;
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    int fd;
    auto[tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    inodeset file_inode_set = tagFS.getInodesFromTags(tag_vec);
    std::string file_path;
    inode new_inode;

    if (fi->flags & O_CREAT) {
        if (!file_inode_set.empty()) {
            errno = EEXIST;
#ifdef DEBUG
            std::cerr << "path already exist: " << path << std::endl;
#endif
            return -errno;
        }
        new_inode = tagFS.getNewInode();
        file_path = std::to_string(new_inode);
    } else {
        file_path = tagFS.getFileRealPath(tag_vec);
    }

    fd = open(file_path.c_str(), fi->flags);
    if (fd == -1)
        return -errno;

    if (fi->flags & O_CREAT) {
        if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
            close(fd);
            unlink(file_path.c_str());
            errno = EEXIST;
            return -errno;
        }
    }

    fi->fh = fd;
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

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

static int xmp_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
                         off_t offset, struct fuse_file_info *fi) {
    struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    (void) path;

    dst.buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    dst.buf[0].fd = fi->fh;
    dst.buf[0].pos = offset;

    return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int xmp_statfs(const char *path, struct statvfs *stbuf) {
    int res;
    auto[tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tag_vec);

    res = statvfs(file_path.c_str(), stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

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

static int xmp_release(const char *path, struct fuse_file_info *fi) {
    (void) path;
    close(fi->fh);

    return 0;
}

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

