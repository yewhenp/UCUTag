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
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::sub_array;


class TagFS {
private:
    // fields in collections
    const std::string TAG_NAME = "tagname";
    const std::string TAG_TYPE = "tagtype";
    const std::string _ID      = "_id";
    const std::string SET      = "$set";
    const std::string INODES   = "inodes";
    const std::string PUSH     = "$push";
    const std::string PULL     = "$pull";
    const std::string IN       = "$in";
    const std::string TAGS     = "tags";
    const std::string FILENAME = "filename";
    const std::string GROUP    = "group";

    // helpers structures for interaction with db
    mongocxx::instance instance{}; // This should be done only once.
    mongocxx::uri uri{"mongodb://localhost:27017"};
    mongocxx::client client;
    mongocxx::database db;

    // collections in DB
    mongocxx::collection tags;
    mongocxx::collection tagToInode;
    mongocxx::collection inodeToTag;
    mongocxx::collection inodetoFilename;

    std::hash<std::string> hasher;
    inline int collectionDelete(mongocxx::collection &collection, num_t id);


public:
    TagFS();
    std::pair<tagvec, int> parseTags(const char *path);
    num_t getFileInode(tagvec &tags);
    std::string getFileRealPath(tagvec &tags);
    inodeset getInodesFromTags(tagvec &tags);
    num_t getNewInode();
    int createNewFileMetaData(tagvec &tags, num_t newInode);
    int deleteFileMetaData(tagvec &tags, num_t fileInode);
    int deleteRegularTags(tagvec &tags);
    int createRegularTags(strvec &tagNames);
    int renameFileTag(num_t inode, const std::string &oldTagName, const std::string &newTagName);
    std::pair<tagvec, int> prepareFileCreation(const char *path);

public:
////////////////////////////////////////////  tags collection manipulation  ///////////////////////////////////////////

    num_t tagNameToTagid(const std::string& tagname);
    int tagsAdd(tag_t tag);                                               // -1 if error else 0
    int tagsUpdate(num_t tagId, tag_t newTag);                            // -1 if error else 0
    tag_t tagsGet(num_t tagId);                                           // {} if error
    int tagsDelete(num_t tagId);                                          // -1 if error else 0

//////////////////////////////////////////  tagToInodes collection manipulation  /////////////////////////////////////////////
    strvec tagNamesByTagType(num_t type);

    int tagToInodeInsert(num_t tagId, num_t inodes);
    int tagToInodeUpdate(num_t tagId, const numvec &inodes);
    numvec tagToInodeGet(num_t tagId);
    int tagToInodeDelete(num_t tagId);
    int tagToInodeAddInode(num_t tagId, num_t inode);
    int tagToInodeDeleteInodes(const numvec &inodes);
    bool tagToInodeFind(num_t tagId); // just check if it exists

//////////////////////////////////////////  InodeToTag collection manipulation  /////////////////////////////////////////////
    int inodeToTagInsert(num_t inode, num_t tagsIds);
    int inodeToTagUpdate(num_t inode, const numvec &tagsIds);
    numvec inodeToTagGet(num_t inode);
    int inodeToTagDelete(num_t inode);
    int inodeToTagAddTagId(num_t inode, num_t tagid);
    int inodeToTagDeleteTags(const numvec &tagIds);
    bool inodeToTagFind(num_t inode); // just check if it exists

    //////////////////////////////////////////  InodeToTag collection manipulation  /////////////////////////////////////////////
    int inodetoFilenameInsert(num_t inode, const std::string &filename);
    int inodetoFilenameUpdate(num_t inode, const std::string &filename);
    std::string inodetoFilenameGet(num_t inode);
    int inodetoFilenameDelete(num_t inode);

    num_t new_inode_counter = 0;
    num_t getMaximumInode();
};

extern TagFS tagFS;


#endif //UCUTAG_TAGFS_H
