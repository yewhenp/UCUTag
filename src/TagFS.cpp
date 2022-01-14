#include <cstdlib>
#include <filesystem>

#include "TagFS.h"

std::pair<tagvec, int> TagFS::parseTags(const char *path) {
    tagvec res{};
    auto splitted = split(path, "/");

#ifdef DEBUG
    std::cout << "Splitted: " << splitted << std::endl;
#endif

    for (auto &tagName: splitted) {
        auto tag = tagsGet(tagNameToTagid(tagName));
        if (tag == tag_t{}) {
            errno = ENOENT;
            return {res, -1};
        }
        res.push_back(tag);
    }

#ifdef DEBUG
    std::cout << "Parsed path: " << path << " : " << res << std::endl;
#endif
    return {res, 0};
}

num_t TagFS::getFileInode(numvec &tags) {
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


std::string TagFS::getFileRealPath(numvec &tags) {
    num_t file_inode = getFileInode(tags);
    return std::to_string(file_inode);
}

inodeset TagFS::getInodesFromTags(numvec &tags) {
    inodeset intersect;
    bool i = true;
    for (const auto &tag: tags) {
        auto inodes = tagToInodeGet(tag);
//        std::cout << "for tag " << tag.name << " found inodes " << inodes << std::endl;
        if (i) {
            intersect.insert(inodes.begin(), inodes.end());
            i = false;
        } else {
            inodeset c;
            inodeset inodesset{inodes.begin(), inodes.end()};
            for (auto &inode: intersect) {
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
//    std::cout << "GetNewInode() -> " << new_inode_counter << std::endl;
    return new_inode_counter++;
}

std::tuple<numvec, strvec, int> TagFS::prepareFileCreation(const char *path) {
//    auto[tag_vec, status] = parseTags(path);
    auto tagsNames = split(path, "/");
    auto tagIds = tagFS.convertToTagIds(tagsNames);
    auto count = tagFS.tagsCount(tagIds);
    if (count == tagIds.size()) {
        if (!getInodesFromTags(tagIds).empty()) {
#ifdef DEBUG
            std::cerr << "path already exist: " << path << std::endl;
#endif
            errno = EEXIST;
            return {tagIds, tagsNames, -errno};
        }
        return {tagIds, tagsNames, 0};
    }

    auto lastTag = tagFS.tagsGet(tagIds.back());
    if (lastTag == tag_t{} && count == tagIds.size() - 1) {
        tagsAdd({TAG_TYPE_FILE, tagsNames.back()});
        return {tagIds, tagsNames, 0};
    }
    return {tagIds, tagsNames, -1};

}

int TagFS::createNewFileMetaData(const numvec &tagIds, const strvec &tagNames, num_t newInode) {
    inodetoFilenameUpdate(newInode, tagNames.back());
    inodeToTagUpdate(newInode, tagIds);
    tagToInodeUpdateMany(tagIds, newInode);

//    for (auto &tagId: tagIds) {
//        if (tagToInodeFind(tagId)) {
////            std::cout  << "tagToInodeAddInode(" << tagId << ", " << newInode << ")" << std::endl;
//            tagToInodeAddInode(tagId, newInode);
//        } else {
//#ifdef DEBUG
//            std::cout  << "tagToInodeInsert(" << tagId << ", " << newInode << ")" << std::endl;
//#endif
//            tagToInodeInsert(tagId, newInode);
//        }
//
//        if (inodeToTagFind(newInode)) {
//#ifdef DEBUG
//            std::cout << "inodeToTagAddTagId(" << newInode << ", " << tagId << ")" << std::endl;
//#endif
//            int res = inodeToTagAddTagId(newInode, tagId);
//#ifdef DEBUG
//            std::cout << "RESULT: " << res << std::endl;
//#endif
//        } else {
//#ifdef DEBUG
//            std::cout << "inodeToTagInsert(" << newInode << ", " << tagId << ")" << std::endl;
//#endif
//            inodeToTagInsert(newInode, tagId);
//        }
//    }

    return 0;
}


int TagFS::deleteFileMetaData(const numvec &tagIds, num_t fileInode) {
//    if (tags.back().type != TAG_TYPE_FILE) {
//#ifdef DEBUG
//        std::cout << "last index was not file: " << tags << std::endl;
//#endif
//        return -1;
//    }
//#ifdef DEBUG
//    std::cout << "deleteFileMetaData started" << std::endl;
//#endif
//    auto fileTagId = tagNameToTagid(tags.back().name);
//#ifdef DEBUG
//    std::cout << "tagNameToTagid done" << std::endl;
//#endif
//    tagToInodeDeleteInodes({fileInode});
//#ifdef DEBUG
//    std::cout << "tagToInodeDeleteInodes done" << std::endl;
//#endif
//    inodeToTagDelete(fileInode);
//#ifdef DEBUG
//    std::cout << "inodeToTagDelete done" << std::endl;
//#endif
//    inodetoFilenameDelete(fileInode);
//#ifdef DEBUG
//    std::cout << "inodetoFilenameDelete done" << std::endl;
//#endif
//    auto residualInodes = tagToInodeGet(fileTagId);
//    if (residualInodes.empty()) {
//        inodeToTagDeleteTags({static_cast<long>(fileTagId)});
//        tagToInodeDelete(fileTagId);
//        tagsDelete(fileTagId);
//    }

    auto fileTagId = tagIds.back();
//    tagToInodeDeleteInodes({fileInode});
    tagToInodeDeleteMany(tagIds, fileInode);
    inodeToTagDelete(fileInode);
    inodetoFilenameDelete(fileInode);
    auto residualInodes = tagToInodeGet(fileTagId);
    if (residualInodes.empty()) {
        tagToInodeDelete(fileTagId);
        tagsDelete(fileTagId);
    }

    return 0;
}

int TagFS::deleteRegularTag(tag_t tag) {
    // check if tags exist and are regular

    if (tag.type != TAG_TYPE_REGULAR) {
        errno = ENOTDIR;
        return -1;
    }
    auto tagId = tagNameToTagid(tag.name);
    inodeToTagDeleteMany(tagToInodeGet(tagId), tagId);
    tagToInodeDelete(tagId);
    tagsDelete(tagId);
    return 0;
}

int TagFS::createRegularTags(strvec &tagNames) {
    auto tagIds = tagFS.convertToTagIds(tagNames);
    auto status = tagFS.tagsCount(tagIds);
    if (status == tagNames.size()) {
        errno = EEXIST;
        return -1;
    }
    if (status < tagNames.size() - 1) {
        errno = EINVAL;
        return -1;
    }
    // TODO: replace loop but since size of tagNames is rarely bigger than 1 we can skip it for now
//    for (auto &tagName: tagNames) {
////        tagToInodeInsert(tagIds[i], -1);
//        tagsAdd({TAG_TYPE_REGULAR, tagName});
//    }
    tagsAdd({TAG_TYPE_REGULAR, tagNames.back()});
    return 0;
}

TagFS::TagFS() {
    const char *env_p = std::getenv("UCUTAG_FILE_DIR");
    std::string file_dir;
    if (env_p == nullptr) {
        file_dir = "/opt/ucutag/files";
    } else {
        file_dir = env_p;
        if (file_dir.back() == '/') {
            file_dir.pop_back();
        }
    }
    if (!std::filesystem::exists(file_dir)) {
        if (!std::filesystem::create_directories(file_dir)) {
            std::cerr << "Unable to create dir" << std::endl;
            std::exit(-1);
        }
    }
    int rc = chdir(file_dir.c_str());
    if (rc < 0) {
        std::cerr << "Unable to enter dir" << std::endl;
        std::exit(-1);
    }
    std::string mong_path = "ucutag" + file_dir;
    std::replace(mong_path.begin(), mong_path.end(), '/', '_');

    client = mongocxx::client(uri);
    db = client[mong_path];
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
    return (num_t) hasher(tagname);
}

int TagFS::tagsAdd(tag_t tag) {
    auto tagId = tagNameToTagid(tag.name);
    bsoncxx::document::value doc_value = document{} <<
                                                    _ID << tagId << TAG_NAME << tag.name << TAG_TYPE << tag.type
                                                    << finalize;
    auto res = tags.insert_one(doc_value.view());
    if (!res)
        return -1;
    return 0;
}

int TagFS::tagsUpdate(num_t tagId, tag_t newTag) {
    mongocxx::options::update options;
    options.upsert(true);
    auto res = tags.update_one(document{} << _ID << (ssize_t) tagId << finalize,
                               document{} << SET << open_document <<
                                          TAG_NAME << newTag.name << TAG_TYPE << newTag.type <<
                                          close_document << finalize, options);
    if (!res)
        return -1;
    return 0;
}

tag_t TagFS::tagsGet(num_t tagId) {
    auto res = tags.find_one(
            document{} << _ID << tagId << finalize);
#ifdef DEBUG
    std::cout << "Tag id: " << tagId << std::endl;
#endif
    if (res) {
        bsoncxx::document::view view = res->view();
        return {.type=view[TAG_TYPE].get_int64(),
                .name=view[TAG_NAME].get_utf8().value.to_string(),};
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
    for (const auto &doc: cursor) {
        result.push_back(doc[TAG_NAME].get_utf8().value.to_string());
    }
    return result;
}

int TagFS::tagsExist(const numvec &tagIds) {
    if (tagsCount(tagIds) != tagIds.size()) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

num_t TagFS::tagsCount(const numvec &tagIds) {
    if (tagIds.empty()) {
        return -1;
    }
    document builder{};
    auto in_array = builder << _ID << open_document << IN << open_array;
    for (auto tagId: tagIds) {
        in_array << tagId;
    }
    auto after_array = in_array << close_array;
    auto doc = after_array << close_document << finalize;
    auto res = tags.count_documents(doc.view());
    return res;
}


//////////////////////////////////////////  tags collection manipulation  /////////////////////////////////////////////
int TagFS::tagToInodeInsert(num_t tagId, num_t inode) {
//    bsoncxx::document::value doc_value;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res;
    if (inode >= 0) {
        auto doc_value = document{} << _ID << tagId << INODES << open_array << inode << close_array << finalize;
        res = tagToInode.insert_one(doc_value.view());
    } else {
        auto doc_value = document{} << _ID << tagId << INODES << open_array << close_array << finalize;
        res = tagToInode.insert_one(doc_value.view());
    }
    if (!res)
        return -1;
    return 0;
}

int TagFS::tagToInodeUpdate(num_t tagId, const numvec &inodes) {
    mongocxx::options::update options;
    options.upsert(true);
    document builder{};
    auto array = builder << SET << open_document << INODES << open_array;
    for (auto inode: inodes) {
        array << inode;
    }
    auto doc = array << close_array << close_document << finalize;
    auto res = inodeToTag.update_one(document{} << _ID << tagId << finalize,
                                     doc.view(),
                                     options);
    if (!res)
        return -1;
    return 0;
}


int TagFS::tagToInodeUpdateMany(const numvec &tagIds, num_t inode) {
    mongocxx::options::update options;
    options.upsert(true);
    for (auto tagId: tagIds) {
        tagToInode.update_many(document{} << _ID << tagId << finalize,
                               document{} << ADD_TO_SET << open_document << INODES << inode << close_document << finalize,
                               options);
    }
    return 0;
}
// db.tagToInode.updateMany({_id: {$in : [Long("-5332547564057496584"), Long("-1895780966306670931"), Long("4017852018407011111")]}}, {inodes:{$push:Long("0")}}, {upsert:true})
// db.tagToInode.updateOne({_id: Long("-5332547564057496584")}, {inodes:{$push:Long("0")}}, {upsert:true})
int TagFS::tagToInodeDeleteMany(const numvec &tagIds, num_t inode) {
    document builder{};
    auto in_array = builder << _ID << open_document << IN << open_array;
    for (auto tagId: tagIds) {
        in_array << tagId;
    }
    auto after_array = in_array << close_array;
    auto doc = after_array << close_document << finalize;
    tagToInode.update_many(doc.view(),
                           document{} << PULL << open_document << INODES << inode << close_document << finalize);
    return 0;
}

bool TagFS::tagToInodeFind(num_t tagId) {
    auto res = tagToInode.find_one(
            document{} << _ID << tagId << finalize);
    if (res) {
        return true;
    }
    return false;
}

numvec TagFS::tagToInodeGet(num_t tagId) {
    auto res = tagToInode.find_one(
            document{} << _ID << tagId << finalize);
    if (res) {
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
    auto doc = bsoncxx::builder::basic::document{};
    doc.append(kvp(IN, [&inodes](sub_array child) {
        for (const auto &inode: inodes) {
            child.append(inode);
        }
    }));
    auto pull_query_finalized = document{} << PULL << open_document << INODES << doc << close_document << finalize;

    auto res = tagToInode.update_many({}, pull_query_finalized.view());

    if (!res)
        return -1;
    return 0;
}


//////////////////////////////////////////  InodeToTag collection manipulation  /////////////////////////////////////////////

int TagFS::inodeToTagInsert(num_t inode, num_t tagsId) {
    bsoncxx::stdx::optional<mongocxx::result::insert_one> res;
    auto doc_value = document{} << _ID << inode << TAGS << open_array << tagsId << close_array << finalize;
    res = inodeToTag.insert_one(doc_value.view());
    if (!res)
        return -1;
    return 0;
}

//int TagFS::inodeToTagInsertMany(num_t inode, numvec tagsIds) {
//    bsoncxx::stdx::optional<mongocxx::result::insert_one> res;
//    auto doc_value = document{} << _ID << inode << TAGS << open_array << tagsId << close_array << finalize;
//    res = inodeToTag.insert_one( doc_value.view() );
//    if (!res)
//        return -1;
//    return 0;
//}

int TagFS::inodeToTagUpdate(num_t inode, const numvec &tagsIds) {
    mongocxx::options::update options;
    options.upsert(true);
    document builder{};
    auto array = builder << SET << open_document << TAGS << open_array;
    for (auto tagId: tagsIds) {
        array << tagId;
    }
    auto doc = array << close_array << close_document << finalize;

    auto res = inodeToTag.update_one(document{} << _ID << inode << finalize, doc.view(), options);
//    document builder{};
//    auto in_array = builder << _ID << inode << TAGS << open_array;
//    for (auto tagId: tagsIds) {
//        in_array << tagId;
//    }
//    auto doc = in_array << close_array << finalize;
//
//    auto res = inodeToTag.insert_one(doc.view());

    if (!res)
        return -1;
    return 0;
}

bool TagFS::inodeToTagFind(num_t inode) {
    auto res = inodeToTag.find_one(
            document{} << _ID << inode << finalize);
    if (res) {
        return true;
    }
    return false;
}

numvec TagFS::inodeToTagGet(num_t inode) {
    auto res = inodeToTag.find_one(
            document{} << _ID << inode << finalize);
    if (res) {
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
    auto res = inodeToTag.update_one(
            document{} << _ID << inode << finalize,
            document{} << PUSH << open_document << TAGS << tagid << close_document << finalize);
    if (!res)
        return -1;
    return 0;
}

int TagFS::inodeToTagDeleteTags(const numvec &tagIds) {
    auto doc = bsoncxx::builder::basic::document{};
    doc.append(kvp(IN, [&tagIds](sub_array child) {
        for (const auto &tagid: tagIds) {
            child.append(tagid);
        }
    }));
    auto pull_query_finalized = document{} << PULL << open_document << INODES << doc << close_document << finalize;
    auto res = inodeToTag.update_many({}, pull_query_finalized.view());
    if (!res)
        return -1;
    return 0;
}

int TagFS::inodeToTagDeleteMany(const numvec &inodes, num_t tagId) {
    document builder{};
    auto in_array = builder << _ID << open_document << IN << open_array;
    for (auto tagId: inodes) {
        in_array << tagId;
    }
    auto after_array = in_array << close_array;
    auto doc = after_array << close_document << finalize;
    tagToInode.update_many(doc.view(),
                           document{} << PULL << open_document << TAGS << tagId << close_document << finalize);
    return 0;
}

int TagFS::inodeToTagUpdateMany(const numvec &inodes, num_t tagIdFrom, num_t tagIdTo) {
    document builder{};
    auto in_array = builder << _ID << open_document << IN << open_array;
    for (auto tagId: inodes) {
        in_array << tagId;
    }
    auto after_array = in_array << close_array;
    auto doc = after_array << close_document << finalize;
    tagToInode.update_many(doc.view(),
                           document{} << PULL << open_document << TAGS << tagIdFrom << close_document << finalize);
    tagToInode.update_many(doc.view(),
                           document{} << PUSH << open_document << TAGS << tagIdTo << close_document << finalize);
    return 0;
}

//////////////////////////////////////////  InodeToTag collection manipulation  /////////////////////////////////////////////

int TagFS::inodetoFilenameInsert(num_t inode, const std::string &filename) {
    auto res = inodetoFilename.insert_one(
            document{} << _ID << inode << FILENAME << filename << finalize);
    if (!res)
        return -1;
    return 0;
}

int TagFS::inodetoFilenameUpdate(num_t inode, const std::string &filename) {
    mongocxx::options::update options;
    options.upsert(true);
    auto res = inodetoFilename.update_one(
            document{} << _ID << inode << finalize,
            document{} << SET << open_document << FILENAME << filename << close_document << finalize, options);
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

num_t TagFS::getMaximumInode() {
    auto sort_order = document{} << _ID << -1 << finalize;
    auto opts = mongocxx::options::find{};
    opts.sort(sort_order.view());

    auto cursor = inodeToTag.find({}, opts);

    for (const auto &doc: cursor) {
        return doc[_ID].get_int64() + 1;
    }

    return 0;
}

int TagFS::renameFileTag(num_t inode, const std::string &oldTagName, const std::string &newTagName) {
    // TODO: Support multiple file having the same filetag
    auto oldTagId = tagNameToTagid(oldTagName);
    auto newTagId = tagNameToTagid(newTagName);
    auto oldTags = inodeToTagGet(inode);

    oldTags.erase(std::remove(oldTags.begin(), oldTags.end(), oldTagId), oldTags.end());
    oldTags.push_back(tagNameToTagid(newTagName));
    inodeToTagUpdate(inode, oldTags);
    inodetoFilenameUpdate(inode, newTagName);

    auto oldInodes = tagToInodeGet(oldTagId);
    tagToInodeDelete(oldTagId);
    for (auto &oldInode: oldInodes) {
        tagToInodeAddInode(newTagId, oldInode);
    }

    tagsDelete(oldTagId);
    tagsAdd({TAG_TYPE_FILE, newTagName});
    return 0;
}

numvec TagFS::convertToTagIds(const char *path) {
    auto tagNames = split(path, "/");
    return convertToTagIds(tagNames);
}

numvec TagFS::convertToTagIds(const strvec &tagNames) {
    numvec tagids{};
    tagids.reserve(tagNames.size());
    for (auto &tag: tagNames) {
        tagids.emplace_back(tagNameToTagid(tag));
    }
    return tagids;
}



