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

#include "string_utils.h"

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

#ifdef DEBUG
    std::cout << " >>> getattr: " << path << std::endl;
#endif

    if ( strcmp( path, "/" ) == 0 || strcmp( path, "/@" ) == 0) {
        fillTagStat(stbuf);
        return 0;
    }

    auto [tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) {
#ifdef DEBUG
        std::cout << " >>> parseTags error " << std::endl;
#endif
        return -errno;
    }

    if ( tag_vec.back().type == TAG_TYPE_REGULAR ) {
        fillTagStat(stbuf);
        return 0;
    }

    std::string file_path = tagFS.getFileRealPath(tag_vec);
    std::cout << "lstating " << file_path << std::endl;
    res = lstat(file_path.c_str(), stbuf);
    std::cout << "lstating done" << std::endl;
    if (res == -1) {
#ifdef DEBUG
        std::cout << " >>> lstat error " << std::endl;
#endif
        return -errno;
    }

    return 0;
}

static int xmp_access(const char *path, int mask) {
#ifdef DEBUG
    std::cout << " >>> access: " << path << std::endl;
#endif
    if ( strcmp( path, "/" ) == 0 ) {
        mask = R_OK | W_OK | X_OK;
        return 0;
    }

    int res;

    auto [tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;

    // return 0 for directory
    if (tag_vec.back().type == TAG_TYPE_REGULAR) {
        return 0;
    }
    std::string file_path = tagFS.getFileRealPath(tag_vec);
//    std::cout << "FILE PATH: " << file_path << std::endl;
    res = access(file_path.c_str(), mask);
    if (res == -1)
        return -errno;

    return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size) {
#ifdef DEBUG
    std::cout << " >>> readlink: " << path << std::endl;
#endif

    int res;

    auto [tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tag_vec);
    res = readlink(file_path.c_str(), buf, size - 1);
    if (res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}

static int xmp_opendir(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> opendir: " << path << std::endl;
#endif

    int res = 0;
    tag_dirp* d;
    try {
        d = new tag_dirp{};
    } catch (std::bad_alloc& err) {
        return -ENOMEM;
    }

#ifdef DEBUG
    std::cerr << " >>> opendir new done" << std::endl;
#endif

    // fill and save pointer to dirent
    auto [tags, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;

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

#ifdef DEBUG
    std::cout << " >>> readdir: " << path << std::endl;
#endif

    int status = 0;
    tag_dirp *d = get_dirp(fi);

    // get first entry
    d->entry = d->inodes.begin();

    // move entry on offset
    std::advance(d->entry, offset);

    while (d->entry != d->inodes.end()) {
        struct stat st{};
        auto filename = tagFS.inodetoFilenameGet(*d->entry);
        if (filename == "@"){
            fillTagStat(&st);
            for(auto& nonFileTagName: tagFS.tagNamesByTagType(TAG_TYPE_REGULAR)){
#ifdef DEBUG
                std::cout << " >>> readdir: " << "non-file tagname: " << nonFileTagName << std::endl;
#endif
                // TODO: find bug: 'getNonFileTags' doesn't filter
//                if (tagFS.tagNameTag[nonFileTagName].type == TAG_TYPE_REGULAR) {
//                    fillTagStat(&st);
//                    filler(buf, nonFileTagName.c_str(), &st, 0);
//                }
                filler(buf, nonFileTagName.c_str(), &st, 0);
            }
        } else {
            if (lstat(std::to_string(*d->entry).c_str(), &st) == -1) status = -1;
            filler(buf, filename.c_str(), &st, 0);
//            std::cout << "lstating " << filename << std::endl;
//            if (lstat(filename.c_str(), &st) == -1) {
//                status = -1;
//                continue;
//            }
//            filler(buf, filename.c_str(), &st, 0);
//            auto inode_tags = tagFS.inodeToTagGet(*d->entry);
//            for (auto& tag_id : inode_tags) {
//                auto tag = tagFS.tagsGet(tag_id);
//                std::cout << "Checking tag " << tag.name << std::endl;
//                if (tag.type == TAG_TYPE_FILE) {
//                    std::cout << "Yes" << std::endl;
//                    filler(buf, tag.name.c_str(), &st, 0);
//                    break;
//                }
//            }
//            filler(buf, *d->entry, &st, 0);
        }


        d->entry++;
    }

    return status;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> release: " << path << std::endl;
#endif

    tag_dirp *d = get_dirp(fi);
    free(d);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
#ifdef DEBUG
    std::cout << " >>> mkdir: " << path << std::endl;
#endif
    strvec tagNames = split(path, "/");
    return tagFS.createRegularTags(tagNames);
}



static int xmp_mknod(const char *path, mode_t mode, dev_t rdev) {
#ifdef DEBUG
    std::cout << " >>> mknod: " << path << std::endl;
#endif

    int res;
    // We expect that all tags exist or last one might not exist
    auto[tag_vec, status] = tagFS.prepareFileCreation(path);
    if (status != 0) {
        return status;

    }
    // TODO: not sure if it's needed: called for creation of all non-directory, non-symlink nodes.
    if (S_ISDIR(mode)) {
        xmp_mkdir(path, mode);
    }
    else {
        num_t new_inode = tagFS.getNewInode();
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
        auto splitted = split(path, "/");

        tag_vec.push_back({TAG_TYPE_FILE, splitted.back()});
        if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
            unlink(new_path.c_str());
            errno = EEXIST;
            return -errno;
        }

    }

    return 0;
}

static int xmp_unlink(const char *path) {
#ifdef DEBUG
    std::cout << " >>> unlink: " << path << std::endl;
#endif

    int res;
    auto [tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tag_vec);
    num_t file_inode = tagFS.getFileInode(tag_vec);
    if (file_inode == num_t(-1)) {
        errno = ENOENT;
        return -errno;
    }

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
#ifdef DEBUG
    std::cout << " >>> rmdir: " << path << std::endl;
#endif

    auto [tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) return -errno;
    return tagFS.deleteRegularTags(tag_vec);
}

static int xmp_symlink(const char *from, const char *to) {
#ifdef DEBUG
    std::cout << " >>> symlink: " << from << " -> " << to << std::endl;
#endif

    int res;

    auto [tag_vec_to, status_to] = tagFS.prepareFileCreation(to);
    if (status_to != 0) return status_to;
    auto [tag_vec_from, status_from] = tagFS.parseTags(from);
    if (status_from != 0) return -errno;
    std::string link_file_path = tagFS.getFileRealPath(tag_vec_from);

    num_t new_inode = tagFS.getNewInode();
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

//// TODO: Replace dir rename with tag rename
static int xmp_rename(const char *from, const char *to) {
#ifdef DEBUG
    std::cout << " >>> rename: " << from << " -> " << to << std::endl;
#endif

    auto[tag_vec_from, status_from] = tagFS.parseTags(from);
    if (status_from != 0) return -errno;

    auto[tag_vec_to, status_to] = tagFS.parseTags(to);
    auto tag_name_to = split(to, "/");

    auto inodes_from = tagFS.getInodesFromTags(tag_vec_from);
    if (inodes_from.size() > 1) {
        errno = ENOENT;
        return -errno;
    }
    if (inodes_from.empty() && tag_vec_from.size() == tag_name_to.size()) {
        // Rename tags one by one
        return 0;
    }
    if (status_to == 0 || (status_to != 0 && tag_vec_to.size() < tag_name_to.size() - 1)) {
        errno = EEXIST;
        return -errno;
    }

    auto inode_from = *inodes_from.begin();

    if (tag_vec_to.size() == tag_name_to.size() - 1) {
        tagFS.renameFileTag(inode_from, tag_vec_from.back().name, tag_name_to.back());
    }
    tagFS.tagToInodeDeleteInodes({inode_from});
    tag_vec_to.push_back({TAG_TYPE_FILE, tag_name_to.back()});
    for (auto &tag: tag_vec_to) {
        auto tagId = tagFS.tagNameToTagid(tag.name);
        if (tagFS.tagToInodeFind(tagId)) {
            tagFS.tagToInodeAddInode(tagId, inode_from);
        }
        else {
            tagFS.tagToInodeInsert(tagId, inode_from);
        }

        if (tagFS.inodeToTagFind(inode_from))
            tagFS.inodeToTagAddTagId(inode_from, tagId);
        else
            tagFS.inodeToTagInsert(inode_from, tagId);
    }

    return 0;
}

static int xmp_link(const char *from, const char *to) {
#ifdef DEBUG
    std::cout << " >>> link: " << from << " -> " << to << std::endl;
#endif

    int res;

    auto [tag_vec_to, status_to] = tagFS.prepareFileCreation(to);
    if (status_to != 0) return status_to;

    auto[tag_vec_from, status_from] = tagFS.parseTags(from);
    if (status_from != 0) return -errno;
    std::string link_file_path = tagFS.getFileRealPath(tag_vec_from);

    num_t new_inode = tagFS.getNewInode();
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
#ifdef DEBUG
    std::cout << " >>> chmod: " << path << std::endl;
#endif

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
#ifdef DEBUG
    std::cout << " >>> chown: " << path << std::endl;
#endif

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
#ifdef DEBUG
    std::cout << " >>> truncate: " << path << std::endl;
#endif

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

//static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
//#ifdef DEBUG
//    std::cout << " >>> create: " << path << std::endl;
//#endif
//
//    int fd;
//
//    auto tag_vec = tagvec();
//    bool last_exist = false;
//    for(auto& tagName: split(path, "/")){
//
//        if (tagFS.tagNameTag.find(tagName) != tagFS.tagNameTag.end()) {
//            tag_vec.push_back(tagFS.tagNameTag[tagName]);
//            last_exist = true;
//        } else {
//            last_exist = false;
//            tag_vec.push_back({TAG_TYPE_REGULAR, tagName});
//        }
//    }
//    if (last_exist) {
//#ifdef DEBUG
//        std::cout << " >>> create exiting because exist: " << path << std::endl;
//#endif
//
//        errno = EEXIST;
//        return -errno;
//    }
//
//
//    num_t new_inode = tagFS.getNewInode();
//    std::string new_path = std::to_string(new_inode);
//
//#ifdef DEBUG
//    std::cout << " >>> create inode got: " << new_path << std::endl;
//#endif
//
//    fd = open(new_path.c_str(), fi->flags, mode);
//    if (fd == -1)
//        return -errno;
//
//#ifdef DEBUG
//    std::cout << " >>> create open done: " << new_path << std::endl;
//#endif
//
//    if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
//        close(fd);
//        unlink(new_path.c_str());
//        errno = EEXIST;
//        return -errno;
//    }
//
//#ifdef DEBUG
//    std::cout << " >>> create file data created: " << new_path << std::endl;
//#endif
//
//    fi->fh = fd;
//    return 0;
//}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> open: " << path << std::endl;
#endif

    int fd;
    std::string file_path;
    num_t new_inode;
    inodeset file_inode_set;
//    tagvec tag_vec;
//    int status;

    if (fi->flags & O_CREAT) {
        auto[tag_vec, status] = tagFS.prepareFileCreation(path);
        if (status != 0)
            return status;
        new_inode = tagFS.getNewInode();
        file_path = std::to_string(new_inode);
        if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
            errno = EEXIST;
            return -errno;
        }
    } else {
        auto[tag_vec, status] = tagFS.parseTags(path);
        if (status != 0) return -errno;
        file_inode_set = tagFS.getInodesFromTags(tag_vec);
        file_path = tagFS.getFileRealPath(tag_vec);
    }

    fd = open(file_path.c_str(), fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> read: " << path << std::endl;
#endif

    int res;

    (void) path;
    res = pread(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
                        size_t size, off_t offset, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> read_buf: " << path << std::endl;
#endif

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
#ifdef DEBUG
    std::cout << " >>> write: " << path << std::endl;
#endif

    int res;

    (void) path;
    res = pwrite(fi->fh, buf, size, offset);
    if (res == -1)
        res = -errno;

    return res;
}

static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
                         off_t offset, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> write_buf: " << path << std::endl;
#endif

    struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    (void) path;

    dst.buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    dst.buf[0].fd = fi->fh;
    dst.buf[0].pos = offset;

    return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int xmp_statfs(const char *path, struct statvfs *stbuf) {
#ifdef DEBUG
    std::cout << " >>> statfs: " << path << std::endl;
#endif

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
#ifdef DEBUG
    std::cout << " >>> flush: " << path << std::endl;
#endif

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
#ifdef DEBUG
    std::cout << " >>> release: " << path << std::endl;
#endif

    (void) path;
    close(fi->fh);

    return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> fsync: " << path << std::endl;
#endif

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
#ifdef DEBUG
    std::cout << " >>> flock: " << path << std::endl;
#endif

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
    tagFS.new_inode_counter = tagFS.get_maximum_inode();
    int fd;
    auto tag_vec = tagvec(1, {TAG_TYPE_REGULAR, "@"});
    num_t new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);
    fd = open(new_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (tagFS.createNewFileMetaData(tag_vec, new_inode) != 0) {
        unlink(new_path.c_str());
    }
    close(fd);


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
//        .create        = xmp_create,
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

//void init_from_dir(const std::string &root){
//    std::size_t inode_count = 0;
//    for(auto& p: fs::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)){
//        tag_t tag;
//        tag.name = *(--p.path().end());
//        if(!std::filesystem::is_directory(p)){
//            tag.type = TAG_NAME;
//        } else {
//            tag.type = FILE_NAME;
//            tagFS.inodeFilenameMap.insert({inode_count, tag.name});
//        }
//        tagFS.inodeTagMap[inode_count].insert(tag);
//        tagFS.tagInodeMap[tag].insert(inode_count);
//        tagFS.tagNameTag.insert({tag.name, tag});
//        inode_count++;
//    }
//
//    std::cout << inode_count << std::endl;
//    std::cout << tagFS.inodeTagMap.size() << std::endl;
//    std::cout << tagFS.tagInodeMap.size() << std::endl;
//    std::cout << tagFS.tagNameTag.size() << std::endl;
//    std::cout << tagFS.inodeFilenameMap.size() << std::endl;
//}

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, nullptr);

}

