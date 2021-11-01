#ifndef UCUTAG_TAGFS_H
#define UCUTAG_TAGFS_H

#include <cstddef>
#include <algorithm>
#include "typedefs.h"
#include "string_utils.h"
#include <iostream>

template<>
struct std::hash<tag_t> {
    std::size_t operator()(const tag_t &k) const {
        return ((hash<string>()(k.name) ^ (hash<std::size_t>()(k.id) << 1)) >> 1) ^ (hash<std::size_t>()(k.type) << 1);
    }
};

class TagFS {
    tagsvec parse_tags(char *path);
private:
    tagInodeMap_t tagInodeMap{};
    inodeTagMap_t inodeTagMap{};
    inodeFilenameMap_t inodeFilenameMap{};
    tagNameTag_t tagNameTag{};
};


#endif //UCUTAG_TAGFS_H
