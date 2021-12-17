#include "TagFS.h"



strvec TagFS::getNonFileTags(){

    strvec res{};
    for(auto const& [tag, inode]: tagInodeMap) {
        if(tag.type == TAG_TYPE_REGULAR) {
            res.push_back(tag.name);
        }
    }
    return res;

}



std::pair<tagvec, int> TagFS::parseTags(const char *path) {
    tagvec res{};
    auto splitted = split(path, "/");

#ifdef DEBUG
    std::cout << "Splitted: " << splitted << std::endl;
#endif


//    if (splitted.empty() || splitted[0] == ".") {
//
//        for(auto const& [tag, inode]: tagInodeMap) {
//            if(tag.type == TAG_TYPE_FILE) {
//                res.push_back(tag);
//            }
//        }
//#ifdef DEBUG
//        std::cout << "Dot parse: " << path << " : " << res << std::endl;
//#endif
//        return {res, 0};
//    }
    for (auto &tagName: splitted) {
        auto tag = tagsGet(tagNameToTagid(tagName));
        if (!(tag == tag_t{})) {
            errno = ENOENT;
            return {res, -1};
        }
        res.push_back(tag);
    }

//    std::transform(splitted.begin(), splitted.end(), std::back_inserter(res),
//                   [this](const std::string &name) -> tag_t {return tagNameTag[name];});
#ifdef DEBUG
    std::cout << "Parsed path: " << path << " : " << res << std::endl;
#endif
    return {res, 0};
}

num_t TagFS::getFileInode(tagvec& tags) {
    inodeset file_inode_set = getInodesFromTags(tags);
    if (file_inode_set.size() > 1) {
#ifdef DEBUG
        std::cerr << "got multiple inodes in file tags: " << tags << std::endl;
#endif
        return static_cast<num_t>(-1);
    }
    if (file_inode_set.empty()) {
#ifdef DEBUG
        std::cout << "got empty inode set in file tags: " << tags << std::endl;
#endif
        return static_cast<num_t>(-1);
    }
    num_t file_inode = *file_inode_set.begin();
    return file_inode;
}


std::string TagFS::getFileRealPath(tagvec& tags) {
    num_t file_inode = getFileInode(tags);
    return std::to_string(file_inode);
}

inodeset TagFS::getInodesFromTags(tagvec &tags) {
    inodeset intersect;
    bool i = true;
    for (const auto &tag: tags) {
        auto inodes = tagToInodeGet(tagNameToTagid(tag.name));
        if (i) {
            intersect.insert(inodes.begin(), inodes.end());
            i = false;
        } else {
            inodeset c;
            inodeset inodesset{inodes.begin(),  inodes.end()};
            for (auto& inode : intersect) {
                if (inodesset.count(inode) > 0) {
                    c.insert(inode);
                }
            }
            intersect = std::move(c);
        }
    }
    return intersect;
}

num_t TagFS::getNewInode() {
    return new_inode_counter++;
}

int TagFS::createNewFileMetaData(tagvec &tags, num_t newInode) {
    auto& fileTag = tags.back();
    fileTag.type = TAG_TYPE_FILE;
    tagsUpdate(tagNameToTagid(fileTag.name), fileTag);

    inodetoFilenameUpdate(newInode, std::to_string(newInode));
    // TODO: remove legacy (we need to allow multiples files with the same filename)
//    if (tagInodeMap.find(tags[tags.size() - 1]) != tagInodeMap.end()) {
//#ifdef DEBUG
//        std::cout << "file exist: " << tags << std::endl;
//#endif
//        return -1;
//    }

    for (auto &tag: tags) {
        auto tagId = tagNameToTagid(tag.name);
        tagToInodeAddInode(tagId, newInode);
        nodeToTagAddTagId(newInode, tagId);
    }
    return 0;
}

int TagFS::deleteFileMetaData(tagvec &tags, num_t fileInode) {
    if (tags.back().type != TAG_TYPE_FILE) {
#ifdef DEBUG
        std::cout << "last index was not file: " << tags << std::endl;
#endif
        return -1;
    }

    inodetoFilenameDelete(fileInode);

    for (auto &tag: nodeToTagGet(fileInode)) {
        auto tagInodeMapGet();
        tagInodeMap[tag].erase(fileInode);
    }
    tagNameTag.erase(tags.back().name);
    tagInodeMap.erase(tags.back());
    inodeTagMap.erase(fileInode);


    return 0;
}

int TagFS::deleteRegularTags(strvec &tagNames) {
    // check if tags exist and are regular
    for (auto &tagName: tagNames) {
        if (tagNameTag.find(tagName) == tagNameTag.end()) {
            errno = ENOENT;
            return -1;
        }
        if (tagNameTag[tagName].type != TAG_TYPE_REGULAR) {
            errno = ENOTDIR;
            return -1;
        }
    }

    for (auto &tagName: tagNames) {
        auto tag = tagNameTag[tagName];
        for (auto &inode: tagInodeMap[tag])
            inodeTagMap[inode].erase(tag);
        tagNameTag.erase(tagName);
        tagInodeMap.erase(tag);
    }
    return 0;
}

int TagFS::createRegularTags(strvec &tagNames) {
    std::vector<size_t> tagIds;
    tagIds.reserve(tagNames.size());
    for (auto &tagName: tagNames) {
        auto tagId = tagNameToTagid(tagName);
        auto tag = tagsGet(tagId);
        if (!(tag == tag_t{})) {
            errno = EEXIST;
            return -1;
        }
        tagIds.emplace_back(tagId);
    }

    for (size_t i = 0; i < tagNames.size(); i++) {
        tagToInodeInsert(tagIds[i], {});
        tagsAdd({TAG_TYPE_REGULAR, tagNames[i]});
    }
    return 0;
}

TagFS::TagFS() {
    mongocxx::instance instance{}; // This should be done only once.
    mongocxx::uri uri("mongodb://localhost:27017");
    mongocxx::client client(uri);

    db = client["ucutag"];
    tags = db["tags"];
    tagToInode = db["tagToInode"];
    inodeToTag = db["inodeToTag"];
    inodetoFilename = db["inodetoFilename"];
}



int TagFS::collectionDelete(mongocxx::collection &collection, num_t id) {
    auto res = collection.delete_one(document{} << _ID << id << finalize);
    if (!res)
        return -1;
    return 1;
}



////////////////////////////////////////////  tags collection manipulation  /////////////////////////////////////////////

std::size_t TagFS::tagNameToTagid(const std::string &tagname) {
    return hasher(tagname);
}

int TagFS::tagsAdd(tag_t tag) {
    auto tagId = tagNameToTagid(tag.name);
    bsoncxx::document::value doc_value = document{} <<
            _ID << tagId << TAG_NAME << tag.name << TAG_TYPE << tag.type << finalize;
    auto res = tags.insert_one(doc_value.view());
    if (!res)
        return -1;
    return 0;
}

int TagFS::tagsUpdate(num_t tagId, tag_t newTag) {
    auto res = tags.update_one(document{} << _ID << (ssize_t)tagId << finalize,
                    document{} << SET << open_document <<
                    TAG_NAME << newTag.name << TAG_TYPE << newTag.type <<
                    close_document << finalize);
    if (!res)
        return -1;
    return 0;
}

tag_t TagFS::tagsGet(num_t tagId) {
    auto res = tags.find_one(
            document{} << _ID << tagId << finalize);
    if(res) {
        bsoncxx::document::view view = res->view();
        return { .type=static_cast<size_t>(view[TAG_TYPE].get_int64()),
                 .name=view[TAG_NAME].get_utf8().value.to_string(), };
    }
    return {};
}

int TagFS::tagsDelete(num_t tagId) {
    return collectionDelete(tags, tagId);
}


strvec TagFS::tagNamesByTagType(num_t type) {
    strvec result;
    mongocxx::cursor cursor = tags.find(
            document{} << TAG_TYPE << type << finalize);
    for(auto doc : cursor) {
        result.push_back(doc[TAG_NAME].get_utf8().value.to_string());
    }
    return result;
}


//////////////////////////////////////////  tags collection manipulation  /////////////////////////////////////////////

int TagFS::tagToInodeInsert(num_t tagId, const numvec &inodes) {
    auto doc_value = document{} << _ID << tagId << INODES << open_array;
    for (const num_t inode: inodes) {
        doc_value = doc_value << inode;
    }
    auto doc_finalized = doc_value << close_array << finalize;

    auto res = tagToInode.insert_many( (doc_finalized).view());
    if (!res)
        return -1;
    return 0;
}

int TagFS::tagToInodeUpdate(num_t tagId, const numvec &inodes) {
    auto doc_value = document{} << _ID << tagId << INODES << open_array;
    for (const num_t inode: inodes) {
        doc_value = doc_value << inode;
    }
    auto doc_finalized = doc_value << close_array << finalize;
    auto res = tagToInode.update_one(document{} << _ID << tagId << finalize, doc_finalized.view());
    if (!res)
        return -1;
    return 0;
}

numvec TagFS::tagToInodeGet(num_t tagId) {
    auto res = tagToInode.find_one(
            document{} << _ID << tagId << finalize);
    if(res) {
        auto view = res->view();
        auto arr_value = view[INODES].get_array().value;

        return numvec{arr_value.begin(), arr_value.end()};
    }
    return {};
}

int TagFS::tagToInodeDelete(num_t tagId) {
    return collectionDelete(tagToInode, tagId);
}

int TagFS::tagToInodeAddInode(num_t tagId, num_t inode) {
    auto res = tagToInode.update_one(
            document{} << _ID << tagId << finalize,
            document{} << PUSH << open_document << INODES << inode << close_document << finalize);
    if (!res)
        return -1;
    return 0;
}

int TagFS::tagToInodeDeleteInodes(const numvec &inodes) {
    auto pull_query = document{} << PULL << open_document << INODES << open_document << IN << open_array;
    for (const auto &inode: inodes) {
        pull_query = pull_query << inode;
    }
    auto res = tagToInode.update_many(document{} << finalize,
                                      pull_query << close_array << close_document << close_document << finalize);
    if (!res)
        return -1;
    return 0;
}


//////////////////////////////////////////  InodeToTag collection manipulation  /////////////////////////////////////////////

int TagFS::inodeToTagInsert(num_t inode, const numvec &tagsIds) {
    auto doc_value = document{} << _ID << inode << TAGS << open_array;
    for (const num_t tagid: tagsIds) {
        doc_value = doc_value << tagid;
    }
    auto doc_finalized = doc_value << close_array << finalize;
    auto res = inodeToTag.insert_many( doc_finalized.view());
    if (!res)
        return -1;
    return 0;
}

int TagFS::inodeToTagUpdate(num_t inode, const numvec &tagsIds) {
    auto doc_value = document{} << _ID << inode << TAGS << open_array;
    for (const num_t tagid: tagsIds) {
        doc_value = doc_value << tagid;
    }
    auto doc_finalized = doc_value << close_array << finalize;
    auto res = inodeToTag.update_one(document{} << _ID << inode << finalize, doc_finalized.view());
    if (!res)
        return -1;
    return 0;
}

numvec TagFS::inodeToTagGet(num_t inode) {
    auto res = inodeToTag.find_one(
            document{} << _ID << inode << finalize);
    if(res) {
        auto view = res->view();
        auto arr_value = view[TAGS].get_array().value;

        return numvec{arr_value.begin(), arr_value.end()};
    }
    return {};
}

int TagFS::inodeToTagDelete(num_t inode) {
    return collectionDelete(inodeToTag, inode);
}

int TagFS::inodeToTagAddTagId(num_t inode, num_t tagid) {
    auto res = tagToInode.update_one(
            document{} << _ID << inode << finalize,
            document{} << PUSH << open_document << TAGS << tagid << close_document << finalize);
    if (!res)
        return -1;
    return 0;
}

int TagFS::inodeToTagDeleteTags(const numvec &tagIds) {
    auto pull_query = document{} << PULL << open_document << TAGS << open_document << IN << open_array;
    for (const auto &tagid: tagIds) {
        pull_query = pull_query << tagid;
    }
    auto res = tagToInode.update_many(document{} << finalize,
                                      pull_query << close_array << close_document << close_document << finalize);
    if (!res)
        return -1;
    return 0;
}

//////////////////////////////////////////  InodeToTag collection manipulation  /////////////////////////////////////////////

int TagFS::inodetoFilenameInsert(num_t inode, const std::string &filename) {
    auto res = inodetoFilename.insert_one(
            document{} << _ID << inode << FILENAME << filename << finalize);
    if (!res)
        return -1;
    return 0;}

int TagFS::inodetoFilenameUpdate(num_t inode, const std::string &filename) {
    auto res = inodetoFilename.update_one(
            document{} << _ID << inode << finalize,
            document{} << FILENAME << filename << finalize);
    if (!res)
        return -1;
    return 0;
}

std::string TagFS::inodetoFilenameGet(num_t inode) {
    auto res = inodetoFilename.find_one(
            document{} << _ID << inode << finalize);
    if (res) {
        bsoncxx::document::view view = res->view();
        return view[FILENAME].get_utf8().value.to_string();
    }
    return {};
}

int TagFS::inodetoFilenameDelete(num_t inode) {
    return collectionDelete(inodetoFilename, inode);
}


