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
    tagvec parse_tags(const char *path);
    inodeset select(const char *path, bool cache = false);

public:
    tagInodeMap_t tagInodeMap{};
    inodeTagMap_t inodeTagMap{};
    inodeFilenameMap_t inodeFilenameMap{};
    tagNameTag_t tagNameTag{};
};

extern TagFS tagFS;


#endif //UCUTAG_TAGFS_H
