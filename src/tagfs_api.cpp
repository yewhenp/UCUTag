#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "tagfs_api.h"
#include "string_utils.h"
#include "TagFS.h"

TagFS tagFS{};

struct tag_dirp {
    inodeset inodes;
    inodeset::iterator entry;
};

static int ucutag_getattr(const char *path, struct stat *stbuf) {
#ifdef DEBUG
    std::cout << " >>> getattr: " << path << std::endl;
#endif

    int res;
    if (strcmp(path, "/") == 0 || strcmp(path, "/@") == 0) {
        fillTagStat(stbuf);
        return 0;
    }
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
//    auto [tag_vec, status] = tagFS.parseTags(path);
    if (status != 0) {
#ifdef DEBUG
        std::cout << " >>> parseTags error " << std::endl;
#endif
        return -errno;
    }
    auto lastTag = tagFS.tagsGet(tagIds.back());
    if (lastTag.type == TAG_TYPE_REGULAR) {
        fillTagStat(stbuf);
        return 0;
    }

    std::string filePath = tagFS.getFileRealPath(tagIds);
    res = lstat(filePath.c_str(), stbuf);
    if (res == -1) {
#ifdef DEBUG
        std::cout << " >>> lstat error " << std::endl;
#endif
        return -errno;
    }

    return 0;
}

static int ucutag_access(const char *path, int mask) {
#ifdef DEBUG
    std::cout << " >>> access: " << path << std::endl;
#endif
    if (strcmp(path, "/") == 0) {
        mask = R_OK | W_OK | X_OK;
        return 0;
    }

    int res;

//    auto [tag_vec, status] = tagFS.parseTags(path);
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) return -errno;

    // return 0 for directory
    auto lastTag = tagFS.tagsGet(tagIds.back());
    if (lastTag.type == TAG_TYPE_REGULAR) {
        return 0;
    }
    std::string file_path = tagFS.getFileRealPath(tagIds);
    res = access(file_path.c_str(), mask);
    if (res == -1) return -errno;
    return 0;
}

static int ucutag_readlink(const char *path, char *buf, size_t size) {
#ifdef DEBUG
    std::cout << " >>> readlink: " << path << std::endl;
#endif

    ssize_t res;

//    auto [tag_vec, status] = tagFS.parseTags(path);
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tagIds);
    res = readlink(file_path.c_str(), buf, size - 1);
    if (res == -1) return -errno;

    buf[res] = '\0';
    return 0;
}

static int ucutag_opendir(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> opendir: " << path << std::endl;
#endif

    int res = 0;
    tag_dirp *d;
    try {
        d = new tag_dirp{};
    } catch (std::bad_alloc &err) {
        return -ENOMEM;
    }

#ifdef DEBUG
    std::cerr << " >>> opendir new done" << std::endl;
#endif

    // fill and save pointer to dirent
//    auto [tags, status] = tagFS.parseTags(path);
    if (strcmp(path, "/") == 0) {
        d->inodes = {};
    } else {
        auto tagIds = tagFS.convertToTagIds(path);
        auto status = tagFS.tagsExist(tagIds);
        if (status != 0) return -errno;
        d->inodes = std::move(tagFS.getInodesFromTags(tagIds));
    }

    d->entry = d->inodes.begin();
    fi->fh = (unsigned long) d;
    return res;
}

static inline struct tag_dirp *get_dirp(struct fuse_file_info *fi) {
    return (struct tag_dirp *) (uintptr_t) fi->fh;
}

static int ucutag_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
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
        if (filename == "/") {
            fillTagStat(&st);
            for (auto &regularTagName: tagFS.tagNamesByTagType(TAG_TYPE_REGULAR)) {
#ifdef DEBUG
                std::cout << " >>> readdir: " << "non-file tagname: " << regularTagName << std::endl;
#endif
                filler(buf, regularTagName.c_str(), &st, 0);
            }
        } else {
            if (lstat(std::to_string(*d->entry).c_str(), &st) == -1) status = -1;
            filler(buf, filename.c_str(), &st, 0);
        }


        d->entry++;
    }

    return status;
}

static int ucutag_releasedir(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> release: " << path << std::endl;
#endif
    tag_dirp *d = get_dirp(fi);
    free(d);
    return 0;
}

static int ucutag_mkdir(const char *path, mode_t mode) {
#ifdef DEBUG
    std::cout << " >>> mkdir: " << path << std::endl;
#endif
    strvec tagNames = split(path, "/");
    return tagFS.createRegularTags(tagNames);
}

static int ucutag_mknod(const char *path, mode_t mode, dev_t rdev) {
#ifdef DEBUG
    std::cout << " >>> mknod: " << path << std::endl;
#endif

    int res;
    // We expect that all tags exist or last one might not exist
    auto[tagIds, tagNames, status] = tagFS.prepareFileCreation(path);
    if (status != 0) {
        return status;

    }

    num_t new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);

    if (S_ISFIFO(mode)) {
        res = mkfifo(new_path.c_str(), mode);
    } else {
        res = mknod(new_path.c_str(), mode, rdev);
    }
    if (res == -1) {
        return -errno;
    }
//    auto splitted = split(path, "/");
//
//    tag_vec.push_back({TAG_TYPE_FILE, splitted.back()});
    if (tagFS.createNewFileMetaData(tagIds, tagNames, new_inode) != 0) {
        unlink(new_path.c_str());
        errno = EEXIST;
        return -errno;
    }


    return 0;
}

static int ucutag_unlink(const char *path) {
#ifdef DEBUG
    std::cout << " >>> unlink: " << path << std::endl;
#endif

    int res;
//    auto[tag_vec, status] = tagFS.parseTags(path);
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) {
        errno = ENOENT;
        return -errno;
    }
    num_t fileInode = tagFS.getFileInode(tagIds);
    if (fileInode == num_t(-1)) {
        errno = ENOENT;
        return -errno;
    }

    if (tagFS.deleteFileMetaData(tagIds, fileInode) != 0) {
        errno = ENOENT;
        return -errno;
    }

    res = unlink(std::to_string(fileInode).c_str());
    if (res == -1)
        return -errno;

    return 0;
}

static int ucutag_rmdir(const char *path) {
#ifdef DEBUG
    std::cout << " >>> rmdir: " << path << std::endl;
#endif

//    auto[tag_vec, status] = tagFS.parseTags(path);
//    if (status != 0) return -errno;
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) {
        errno = ENOENT;
        return -errno;
    }
    auto tag = tagFS.tagsGet(tagIds.back());
    if (tag == tag_t{}) {
        errno = ENOENT;
        return -errno;
    }
    return tagFS.deleteRegularTag(tag);
}

static int ucutag_symlink(const char *from, const char *to) {
#ifdef DEBUG
    std::cout << " >>> symlink: " << from << " -> " << to << std::endl;
#endif

    int res;

    auto[tagIdsTo, tagNamesTo, status_to] = tagFS.prepareFileCreation(to);
    if (status_to != 0) return status_to;
//    auto[tag_vec_from, status_from] = tagFS.parseTags(from);
    auto tagIdsFrom = tagFS.convertToTagIds(from);
    auto status_from = tagFS.tagsExist(tagIdsFrom);
    if (status_from != 0) return -errno;
    std::string link_file_path = tagFS.getFileRealPath(tagIdsFrom);

    num_t new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);

    res = symlink(link_file_path.c_str(), new_path.c_str());
    if (res == -1)
        return -errno;

    if (tagFS.createNewFileMetaData(tagIdsTo, tagNamesTo, new_inode) != 0) {
        unlink(new_path.c_str());
        errno = EEXIST;
        return -errno;
    }

    return 0;
}

//// TODO: Replace dir rename with tag rename
static int ucutag_rename(const char *from, const char *to) {
#ifdef DEBUG
    std::cout << " >>> rename: " << from << " -> " << to << std::endl;
#endif

//    auto[tag_vec_from, status_from] = tagFS.parseTags(from);
//    if (status_from != 0) return -errno;
    auto tagNamesFrom = split(from, "/");
    auto tagIdsFrom = tagFS.convertToTagIds(tagNamesFrom);
    auto statusFrom = tagFS.tagsExist(tagIdsFrom);
    if (statusFrom != 0) {
        errno = ENOENT;
        return -errno;
    }
    auto tagNamesTo = split(to, "/");
    auto tagIdsTo = tagFS.convertToTagIds(tagNamesTo);
    auto statusTo = tagFS.tagsCount(tagIdsTo);

    if (tagNamesTo.back() == tagNamesFrom.back()) {
        tagNamesTo.pop_back();
        tagIdsTo.pop_back();
    }

//    if (!statusFrom) {
//        errno = ENOENT;
//        return -errno;
//    }


//    auto[tag_vec_to, status_to] = tagFS.parseTags(to);
//    auto tag_name_to = split(to, "/");

    auto inodesFrom = tagFS.getInodesFromTags(tagIdsFrom);
    if (inodesFrom.size() > 1) {
        errno = ENOENT;
        return -errno;
    }
    if (inodesFrom.empty()) {
        auto tagId = tagIdsFrom.back();
        auto inodes = tagFS.tagToInodeGet(tagId);
        tagFS.inodeToTagUpdateMany(inodes, tagIdsFrom.back(), tagIdsTo.back());
        tagFS.tagToInodeDelete(tagId);
        tagFS.tagsDelete(tagId);
        tagFS.tagToInodeUpdate(tagIdsTo.back(), inodes);
        tagFS.tagsUpdate(tagId, {TAG_TYPE_REGULAR, tagNamesTo.back()});
        return 0;
    }
//    if ((status_to == 0 && !(tag_vec_to.back() == tag_vec_from.back())) ||
//        (status_to != 0 && tag_vec_to.size() < tag_name_to.size() - 1)) {
//        errno = EEXIST;
//        return -errno;
//    }

    auto inodeFrom = *inodesFrom.begin();
    auto fileTagIdFrom = tagFS.tagNameToTagid(tagFS.inodetoFilenameGet(inodeFrom));
    auto lastToTag = tagFS.tagsGet(tagIdsTo.back());
    if (lastToTag == tag_t{}) {
        if (std::equal(tagIdsFrom.begin(), tagIdsFrom.end() - 1, tagIdsTo.begin())) {
//            tagFS.renameFileTag(inodeFrom, tagNamesFrom.back(), tagNamesTo.back());
            auto allInodesFrom = tagFS.tagToInodeGet(fileTagIdFrom);

            if (allInodesFrom.size() == 1) {
                tagFS.tagToInodeDelete(fileTagIdFrom);
            } else {
                tagFS.tagToInodeDeleteMany({fileTagIdFrom}, inodeFrom);
            }
            tagFS.tagToInodeUpdate(tagIdsTo.back(), {inodeFrom});
            tagFS.inodeToTagUpdateMany(allInodesFrom, fileTagIdFrom, tagIdsTo.back());
        }
    } else if (lastToTag.type == TAG_TYPE_REGULAR) {
        tagIdsTo.emplace_back(fileTagIdFrom);
        auto inodesTo = tagFS.getInodesFromTags(tagIdsTo);
        if (!inodesTo.empty()) {
            errno = EEXIST;
            return -errno;
        }
        // Tags reassignment
        auto allTagIdsFrom = tagFS.inodeToTagGet(fileTagIdFrom);
        allTagIdsFrom.erase(std::remove(allTagIdsFrom.begin(), allTagIdsFrom.end(), fileTagIdFrom), allTagIdsFrom.end());
        tagFS.tagToInodeDeleteMany(allTagIdsFrom, inodeFrom);

        tagFS.inodeToTagUpdate(inodeFrom, tagIdsTo);
        tagIdsTo.pop_back();
        tagFS.tagToInodeUpdateMany(tagIdsTo, inodeFrom);
    } else {
        errno = EINVAL;
        return -errno;

    }








//    if (tag_vec_to.size() == tag_name_to.size() - 1) {
//        tagFS.renameFileTag(inode_from, tag_vec_from.back().name, tag_name_to.back());
//    }
//    tagFS.tagToInodeDeleteInodes({inode_from});
//    tag_vec_to.push_back({TAG_TYPE_FILE, tag_name_to.back()});
//    for (auto &tag: tag_vec_to) {
//        auto tagId = tagFS.tagNameToTagid(tag.name);
//        if (tagFS.tagToInodeFind(tagId)) {
//            tagFS.tagToInodeAddInode(tagId, inode_from);
//        } else {
//            tagFS.tagToInodeInsert(tagId, inode_from);
//        }
//
//        if (tagFS.inodeToTagFind(inode_from))
//            tagFS.inodeToTagAddTagId(inode_from, tagId);
//        else
//            tagFS.inodeToTagInsert(inode_from, tagId);
//    }

    return 0;
}

static int ucutag_link(const char *from, const char *to) {
#ifdef DEBUG
    std::cout << " >>> link: " << from << " -> " << to << std::endl;
#endif

    int res;

    auto[tagIdsTo, tagNamesTo, status_to] = tagFS.prepareFileCreation(to);
    if (status_to != 0) return status_to;
//    auto[tag_vec_from, status_from] = tagFS.parseTags(from);
    auto tagIdsFrom = tagFS.convertToTagIds(from);
    auto status_from = tagFS.tagsExist(tagIdsFrom);
    if (status_from != 0) return -errno;
    std::string link_file_path = tagFS.getFileRealPath(tagIdsFrom);

    num_t new_inode = tagFS.getNewInode();
    std::string new_path = std::to_string(new_inode);

    res = link(link_file_path.c_str(), new_path.c_str());
    if (res == -1)
        return -errno;

    if (tagFS.createNewFileMetaData(tagIdsFrom, tagNamesTo, new_inode) != 0) {
        unlink(new_path.c_str());
        errno = EEXIST;
        return -errno;
    }

    return 0;
}

static int ucutag_chmod(const char *path, mode_t mode) {
#ifdef DEBUG
    std::cout << " >>> chmod: " << path << std::endl;
#endif

    int res;
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tagIds);
    res = chmod(file_path.c_str(), mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int ucutag_chown(const char *path, uid_t uid, gid_t gid) {
#ifdef DEBUG
    std::cout << " >>> chown: " << path << std::endl;
#endif

    int res;
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tagIds);

    res = lchown(file_path.c_str(), uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

static int ucutag_truncate(const char *path, off_t size) {
#ifdef DEBUG
    std::cout << " >>> truncate: " << path << std::endl;
#endif

    int res;
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tagIds);

    res = truncate(file_path.c_str(), size);

    if (res == -1)
        return -errno;

    return 0;
}

static int ucutag_open(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> open: " << path << std::endl;
#endif

    int fd;
    std::string file_path;
    num_t new_inode;

    if (fi->flags & O_CREAT) {
        auto[tagIds, tagNames, status] = tagFS.prepareFileCreation("@");
        if (status != 0) {
            errno = EEXIST;
            return -errno;
        }
        new_inode = tagFS.getNewInode();
        file_path = std::to_string(new_inode);
        if (tagFS.createNewFileMetaData(tagIds, tagNames,new_inode) != 0) {
            errno = EEXIST;
            return -errno;
        }
    } else {
        auto tagIds = tagFS.convertToTagIds(path);
        auto status = tagFS.tagsExist(tagIds);
        if (status != 0) return -errno;
        file_path = tagFS.getFileRealPath(tagIds);
    }

    fd = open(file_path.c_str(), fi->flags);
    if (fd == -1)
        return -errno;

    fi->fh = fd;
    return 0;
}

static int ucutag_read(const char *path, char *buf, size_t size, off_t offset,
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

static int ucutag_read_buf(const char *path, struct fuse_bufvec **bufp,
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

static int ucutag_write(const char *path, const char *buf, size_t size,
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

static int ucutag_write_buf(const char *path, struct fuse_bufvec *buf,
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

static int ucutag_statfs(const char *path, struct statvfs *stbuf) {
#ifdef DEBUG
    std::cout << " >>> statfs: " << path << std::endl;
#endif

    int res;
    auto tagIds = tagFS.convertToTagIds(path);
    auto status = tagFS.tagsExist(tagIds);
    if (status != 0) return -errno;
    std::string file_path = tagFS.getFileRealPath(tagIds);

    res = statvfs(file_path.c_str(), stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int ucutag_flush(const char *path, struct fuse_file_info *fi) {
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

static int ucutag_release(const char *path, struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> release: " << path << std::endl;
#endif

    (void) path;
    close(fi->fh);

    return 0;
}

static int ucutag_fsync(const char *path, int isdatasync,
                        struct fuse_file_info *fi) {
#ifdef DEBUG
    std::cout << " >>> fsync: " << path << std::endl;
#endif

    int res;
    (void) path;
    (void) isdatasync;

    res = fsync(fi->fh);
    if (res == -1)
        return -errno;

    return 0;
}

static int ucutag_flock(const char *path, struct fuse_file_info *fi, int op) {
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

void *ucutag_init(struct fuse_conn_info *conn) {
#ifdef __APPLE__
    FUSE_ENABLE_SETVOLNAME(conn);
    FUSE_ENABLE_XTIMES(conn);
#endif
    tagFS.new_inode_counter = tagFS.getMaximumInode();
    // chec if @ already exists
//    if (tagFS.new_inode_counter == 0) {
//        int fd;
////        auto tag_vec = tagvec(1, {TAG_TYPE_REGULAR, "@"});
//        auto[tagIds, tagNames, status] = tagFS.prepareFileCreation("@");
//        if (status != 0) {
//#ifdef DEBUG
//            std::cout << " >>> init: " << "cannot create @" << std::endl;
//#endif
//
//        }
//        num_t new_inode = tagFS.getNewInode();
//        std::string new_path = std::to_string(new_inode);
//        fd = open(new_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
//        if (tagFS.createNewFileMetaData(tagIds, tagNames,new_inode) != 0) {
//            unlink(new_path.c_str());
//        }
//        close(fd);
//    }


    return nullptr;
}

int ucutag_utimens(const char *, const struct timespec tv[2]) {
    return 0;
}

void ucutag_destroy(void *userdata) {}

static struct fuse_operations ucutag_oper = {
        .getattr    = ucutag_getattr,
        .readlink   = ucutag_readlink,
        .mknod      = ucutag_mknod,
        .mkdir      = ucutag_mkdir,
        .unlink     = ucutag_unlink,
        .rmdir      = ucutag_rmdir,
        .symlink    = ucutag_symlink,
        .rename     = ucutag_rename,
        .link       = ucutag_link,
        .chmod      = ucutag_chmod,
        .chown      = ucutag_chown,
        .truncate   = ucutag_truncate,
        .open       = ucutag_open,
        .read       = ucutag_read,
        .write      = ucutag_write,
        .statfs     = ucutag_statfs,
        .flush      = ucutag_flush,
        .release    = ucutag_release,
        .fsync      = ucutag_fsync,
        .opendir    = ucutag_opendir,
        .readdir    = ucutag_readdir,
        .releasedir = ucutag_releasedir,
        .init       = ucutag_init,
        .destroy    = ucutag_destroy,
        .access     = ucutag_access,
        .utimens    = ucutag_utimens,
        .write_buf    = ucutag_write_buf,
        .read_buf    = ucutag_read_buf,
        .flock        = ucutag_flock
};

int main(int argc, char *argv[]) {
    umask(0);
    return fuse_main(argc, argv, &ucutag_oper, nullptr);
}
