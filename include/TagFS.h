#ifndef UCUTAG_TAGFS_H
#define UCUTAG_TAGFS_H

#include <cstddef>
#include <algorithm>
#include "typedefs.h"
#include "string_utils.h"
#include <iostream>

class TagFS {
public:
    TagFS()= default;;
    std::pair<tagvec, int> parseTags(const char *path);
    inode getFileInode(tagvec &tags);
    std::string getFileRealPath(tagvec &tags);
    inodeset getInodesFromTags(tagvec &tags);
    inode getNewInode();
    int createNewFileMetaData(tagvec &tags, inode newInode);
    int deleteFileMetaData(tagvec &tags, inode fileInode);
    int deleteRegularTags(strvec &tagNames);
    int createRegularTags(strvec &tagNames);

public:
    tagInodeMap_t tagInodeMap{};
    inodeTagMap_t inodeTagMap{};
    inodeFilenameMap_t inodeFilenameMap{};
    tagNameTag_t tagNameTag{};

    size_t new_inode_counter = 0;
    strvec getNonFileTags();
};

extern TagFS tagFS;


#endif //UCUTAG_TAGFS_H
