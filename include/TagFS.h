#ifndef UCUTAG_TAGFS_H
#define UCUTAG_TAGFS_H

#include <cstddef>
#include <algorithm>
#include "typedefs.h"
#include "string_utils.h"
#include <iostream>

#include <cstdint>
#include <vector>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;


class TagFS {
public:
    TagFS();
    std::pair<tagvec, int> parseTags(const char *path);
    inum getFileInode(tagvec &tags);
    std::string getFileRealPath(tagvec &tags);
    inodeset getInodesFromTags(tagvec &tags);
    inum getNewInode();
    int createNewFileMetaData(tagvec &tags, inum newInode);
    int deleteFileMetaData(tagvec &tags, inum fileInode);
    int deleteRegularTags(strvec &tagNames);
    int createRegularTags(strvec &tagNames);

public:
    // TODO: caching
    // tagid = hash(tagname)
//    tags:               tagid -> {tagname, tagtype}
//    tagInodeMap:        tagid -> [i1 i2 i3]
//    nodeToTag:          ii -> [tagid1 tagid2 ..]
//    inodetoFilename:    ii -> [filename ]

//    inode_counter - in init max(inode)

    std::size_t tagNameToTagid(std::string tagname);

    void tagsAdd(tag_t tag);
    void tagsUpdate(std::size_t tagId, tag_t newTag);
    tag_t tagsGet(std::size_t tagId);
    void tagsDelete(std::size_t tagId);
    bool tagsExists(std::size_t tagId);

    void tagInodeMapUpdate(std::size_t tagId, std::vector<inum> inodes);
    std::vector<inum> tagInodeMapGet(std::size_t tagId);
    void tagInodeMapDelete(std::size_t tagId);
    void tagInodeMapAddInode(std::size_t tagId, inum val);

    void nodeToTagUpdate(inum key, std::vector<std::size_t> tagsIds);
    std::vector<std::size_t> nodeToTagGet(inum key);
    void nodeToTagDelete(inum key);
    void nodeToTagAddTagId(inum key, std::size_t tagid);

    void inodetoFilenameUpdate(inum key, std::string filename);
    std::string inodetoFilenameGet(inum key);
    void inodetoFilenameDelete(inum key);

    tagInodeMap_t tagInodeMap{};
    inodeTagMap_t inodeTagMap{};
    inodeFilenameMap_t inodeFilenameMap{};
    tagNameTag_t tagNameTag{};

    size_t new_inode_counter = 0;
    strvec getNonFileTags();
};

extern TagFS tagFS;


#endif //UCUTAG_TAGFS_H
