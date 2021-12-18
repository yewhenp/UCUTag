#include "TagFS.h"

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
        if (tag == tag_t{}) {
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
        std::cout << "for tag " << tag.name << " found inodes " << inodes << std::endl;
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
    std::cout << "GetNewInode() -> " << new_inode_counter << std::endl;
    return new_inode_counter++;
}

std::pair<tagvec, int> TagFS::prepareFileCreation(const char *path) {
    auto[tag_vec, status] = parseTags(path);

    // We failed to parse inner component of the path
    auto tagNames = split(path, "/");
    if (!(status == 0 || tag_vec.size() == tagNames.size() - 1)) {
        errno = ENOENT;
        return {tag_vec, -errno};
    }
    // Check if combination of existing tags is unique
    if (status == 0) {
        inodeset file_inode_set = getInodesFromTags(tag_vec);

        if (!file_inode_set.empty()) {
            errno = EEXIST;
#ifdef DEBUG
            std::cerr << "path already exist: " << path << std::endl;
#endif
            return {tag_vec, -errno};
        }
        return {tag_vec, 0};
    }
    return {tag_vec, 0};
}

int TagFS::createNewFileMetaData(tagvec &tags, num_t newInode) {
    auto& fileTag = tags.back();
    fileTag.type = TAG_TYPE_FILE;
    tagsUpdate(tagNameToTagid(fileTag.name), fileTag);

    if (inodetoFilenameGet(newInode).empty()) {
        std::cout << "performing insert" << std::endl;
        inodetoFilenameInsert(newInode, fileTag.name);
    }
    else {
        inodetoFilenameUpdate(newInode, fileTag.name);
    }

    for (auto &tag: tags) {
        auto tagId = tagNameToTagid(tag.name);
        if (tagToInodeFind(tagId)) {
            std::cout  << "tagToInodeAddInode(" << tagId << ", " << newInode << ")" << std::endl;
            tagToInodeAddInode(tagId, newInode);
        } else {
            std::cout  << "tagToInodeInsert(" << tagId << ", " << newInode << ")" << std::endl;
            tagToInodeInsert(tagId, newInode);
        }

        if (inodeToTagFind(newInode)) {
            inodeToTagAddTagId(newInode, tagId);
        } else {
            inodeToTagInsert(newInode, tagId);
        }
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
    auto fileTagId = tagNameToTagid(tags.back().name);
    tagToInodeDeleteInodes({fileInode});
    inodeToTagDelete(fileInode);
    inodetoFilenameDelete(fileInode);
    auto residualInodes = tagToInodeGet(fileTagId);
    if (residualInodes.empty()) {
        inodeToTagDeleteTags({static_cast<long>(fileTagId)});
        tagToInodeDelete(fileTagId);
        tagsDelete(fileTagId);
    }


    return 0;
}

int TagFS::deleteRegularTags(tagvec &tags) {
    // check if tags exist and are regular
    for (auto &tag: tags) {
        if (tag.type != TAG_TYPE_REGULAR) {
            errno = ENOTDIR;
            return -1;
        }
    }
    std::vector<num_t> tagIds;
    tagIds.reserve(tags.size());
    for (auto &tag: tags) {
        auto tagId = tagNameToTagid(tag.name);
        tagIds.push_back(tagId);
        tagToInodeDelete(tagId);
        tagsDelete(tagId);
    }
    inodeToTagDeleteTags(tagIds);
    return 0;
}

int TagFS::createRegularTags(strvec &tagNames) {
    std::vector<num_t> tagIds;
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
        tagToInodeInsert(tagIds[i], -1);
        tagsAdd({TAG_TYPE_REGULAR, tagNames[i]});
    }
    return 0;
}

TagFS::TagFS() {
//    mongocxx::instance instance{}; // This should be done only once.
//    mongocxx::uri uri("mongodb://localhost:27017");
//    mongocxx::client client(uri);
    client = mongocxx::client(uri);
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

num_t TagFS::tagNameToTagid(const std::string &tagname) {
    return (num_t)hasher(tagname);
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
    auto res_find = tags.find_one(
            document{} << _ID << tagId << finalize);
    if (res_find) {
        auto res = tags.update_one(document{} << _ID << (ssize_t)tagId << finalize,
                                   document{} << SET << open_document <<
                                              TAG_NAME << newTag.name << TAG_TYPE << newTag.type <<
                                              close_document << finalize);
        if (!res)
            return -1;
        return 0;
    }
    return tagsAdd(newTag);
}

tag_t TagFS::tagsGet(num_t tagId) {
    auto res = tags.find_one(
            document{} << _ID << tagId << finalize);
    std::cout << "Tag id: " << tagId << std::endl;
    if(res) {
        bsoncxx::document::view view = res->view();
        return { .type=view[TAG_TYPE].get_int64(),
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
    for(const auto &doc : cursor) {
        result.push_back(doc[TAG_NAME].get_utf8().value.to_string());
    }
    return result;
}


//////////////////////////////////////////  tags collection manipulation  /////////////////////////////////////////////
int TagFS::tagToInodeInsert(num_t tagId, num_t inode) {
//    bsoncxx::document::value doc_value;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res;
    if (inode >= 0) {
        auto doc_value = document{} << _ID << tagId << INODES << open_array << inode << close_array << finalize;
        res = tagToInode.insert_one( doc_value.view() );
    }
    else {
        auto doc_value = document{} << _ID << tagId << INODES << open_array << close_array << finalize;
        res = tagToInode.insert_one( doc_value.view() );
    }
    if (!res)
        return -1;
    return 0;

//    for (const num_t inode: inodes) {
//        std::cout << "Inserting inode " << inode << " To tagid " << tagId << std::endl;
//        doc_value = doc_value << inode;
//    }
//    auto doc_finalized = doc_value << close_array << finalize;

//    auto res = tagToInode.insert_one( doc_value.view() );
//    if (!res)
//        return -1;
//    return 0;
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

bool TagFS::tagToInodeFind(num_t tagId) {
    auto res = tagToInode.find_one(
            document{} << _ID << tagId << finalize);
    if(res) {
        return true;
    }
    return false;
}

numvec TagFS::tagToInodeGet(num_t tagId) {
    auto res = tagToInode.find_one(
            document{} << _ID << tagId << finalize);
    if(res) {
        auto view = res->view();
        auto arr_value = view[INODES].get_array().value;

        numvec result{};
        for (const auto &it: arr_value) {
            result.push_back(it.get_int64());
        }
        return result;
//        return numvec{arr_value.begin(), arr_value.end()};
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

int TagFS::inodeToTagInsert(num_t inode, num_t tagsIds) {
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res;
    if (tagsIds >= 0) {
        auto doc_value = document{} << _ID << inode << TAGS << open_array << tagsIds << close_array << finalize;
        res = inodeToTag.insert_one( doc_value.view() );
    }
    else {
        auto doc_value = document{} << _ID << inode << TAGS << open_array << close_array << finalize;
        res = inodeToTag.insert_one( doc_value.view() );
    }
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

bool TagFS::inodeToTagFind(num_t inode) {
    auto res = inodeToTag.find_one(
            document{} << _ID << inode << finalize);
    if(res) {
        return true;
    }
    return false;
}

numvec TagFS::inodeToTagGet(num_t inode) {
    auto res = inodeToTag.find_one(
            document{} << _ID << inode << finalize);
    if(res) {
        auto view = res->view();
        auto arr_value = view[TAGS].get_array().value;

        numvec result{};
        for (const auto &it: arr_value) {
            result.push_back(it.get_int64());
        }
        return result;
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
            document{} << SET << open_document << FILENAME << filename << close_document << finalize);
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


///////////////////////////////////////////////////////////////////////

num_t TagFS::get_maximum_inode() {
    auto sort_order = document{} << _ID << -1 << finalize;
    auto opts = mongocxx::options::find{};
    opts.sort(sort_order.view());

    auto cursor = inodeToTag.find({}, opts);

    for (const auto &doc: cursor) {
        return doc[_ID].get_int64();
    }

    return 0;
}

