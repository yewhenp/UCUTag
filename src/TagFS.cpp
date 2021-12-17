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

inum TagFS::getFileInode(tagvec& tags) {
    inodeset file_inode_set = getInodesFromTags(tags);
    if (file_inode_set.size() > 1) {
#ifdef DEBUG
        std::cerr << "got multiple inodes in file tags: " << tags << std::endl;
#endif
        return static_cast<inum>(-1);
    }
    if (file_inode_set.empty()) {
#ifdef DEBUG
        std::cout << "got empty inode set in file tags: " << tags << std::endl;
#endif
        return static_cast<inum>(-1);
    }
    inum file_inode = *file_inode_set.begin();
    return file_inode;
}


std::string TagFS::getFileRealPath(tagvec& tags) {
    inum file_inode = getFileInode(tags);
    return std::to_string(file_inode);
}

inodeset TagFS::getInodesFromTags(tagvec &tags) {
    inodeset intersect;
    bool i = true;
    for (const auto &tag: tags) {
        auto inodes = tagInodeMapGet(tagNameToTagid(tag.name));
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

inum TagFS::getNewInode() {
    return new_inode_counter++;
}

int TagFS::createNewFileMetaData(tagvec &tags, inum newInode) {
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
        tagInodeMapAddInode(tagId, newInode);
        nodeToTagAddTagId(newInode, tagId);
    }
    return 0;
}

int TagFS::deleteFileMetaData(tagvec &tags, inum fileInode) {
    if (tags[tags.size() - 1].type != TAG_TYPE_FILE) {
#ifdef DEBUG
        std::cout << "last index was not file: " << tags << std::endl;
#endif
        return -1;
    }

    inodeFilenameMap.erase(fileInode);

    for (auto &tag: inodeTagMap[fileInode]) {
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
        tagInodeMapInsert(tagIds[i], {});
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

////////////////////////////////////////////  tags collection manipulation  /////////////////////////////////////////////

std::size_t TagFS::tagNameToTagid(std::string tagname) {
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

int TagFS::tagsUpdate(std::size_t tagId, tag_t newTag) {
    auto res = tags.update_one(document{} << _ID << tagId << finalize,
                    document{} << SET << open_document <<
                    TAG_NAME << newTag.name << TAG_TYPE << newTag.type <<
                    close_document << finalize);
    if (!res)
        return -1;
    return 0;
}

tag_t TagFS::tagsGet(std::size_t tagId) {
    auto res = tags.find_one(
            document{} << _ID << tagId << finalize);
    if(res) {
        bsoncxx::document::view view = res->view();
        return { .type=static_cast<size_t>(view[TAG_TYPE].get_int64()),
                 .name=view[TAG_NAME].get_utf8().value.to_string(), };
    }
    return {};
}

int TagFS::tagsDelete(std::size_t tagId) {
    auto res = tags.delete_one(
            document{} << _ID << tagId << finalize
            );
    if (!res)
        return -1;
    return 0;
}

//////////////////////////////////////////  tags collection manipulation  /////////////////////////////////////////////

void TagFS::tagInodeMapUpdate(std::size_t tagId, const std::vector<inum> &inodes) {

}