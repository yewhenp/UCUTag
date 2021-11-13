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
        if (tagNameTag.find(tagName) == tagNameTag.end()) {
            errno = ENOENT;
            return {res, -1};
        }
        res.push_back(tagNameTag[tagName]);
    }

//    std::transform(splitted.begin(), splitted.end(), std::back_inserter(res),
//                   [this](const std::string &name) -> tag_t {return tagNameTag[name];});
#ifdef DEBUG
    std::cout << "Parsed path: " << path << " : " << res << std::endl;
#endif
    return {res, 0};
}

inode TagFS::getFileInode(tagvec& tags) {
    inodeset file_inode_set = getInodesFromTags(tags);
    if (file_inode_set.size() > 1) {
#ifdef DEBUG
        std::cerr << "got multiple inodes in file tags: " << tags << std::endl;
#endif
        return static_cast<inode>(-1);
    }
    if (file_inode_set.empty()) {
#ifdef DEBUG
        std::cout << "got empty inode set in file tags: " << tags << std::endl;
#endif
        return static_cast<inode>(-1);
    }
    inode file_inode = *file_inode_set.begin();
    return file_inode;
}


std::string TagFS::getFileRealPath(tagvec& tags) {
    inode file_inode = getFileInode(tags);
    return std::to_string(file_inode);
}

inodeset TagFS::getInodesFromTags(tagvec &tags) {
    inodeset intersect;
    bool i = true;
    for (const auto &tag: tags) {
        auto inodes = tagInodeMap[tag];
        if (i) {
            intersect.insert(inodes.begin(), inodes.end());
            i = false;
        } else {
            inodeset c;
            for (auto& inode : intersect) {
                if (inodes.count(inode) > 0) {
                    c.insert(inode);
                }
            }
            intersect = c;
        }
    }
    return intersect;
}

inode TagFS::getNewInode() {
    return new_inode_counter++;
}

int TagFS::createNewFileMetaData(tagvec &tags, inode newInode) {
    tags[tags.size() - 1].type = TAG_TYPE_FILE;
    tagset tset(tags.begin(), tags.end());

    if (tagInodeMap.find(tags[tags.size() - 1]) != tagInodeMap.end()) {
#ifdef DEBUG
        std::cout << "file exist: " << tags << std::endl;
#endif
        return -1;
    }

    inodeFilenameMap[newInode] = tags[tags.size() - 1].name;
    inodeTagMap[newInode] = tset;
    for (auto &tag: tags) {
        tagInodeMap[tag].insert(newInode);
        tagNameTag[tag.name] = tag;
    }
    return 0;
}

int TagFS::deleteFileMetaData(tagvec &tags, inode fileInode) {
    if (tags[tags.size() - 1].type != TAG_TYPE_FILE) {
#ifdef DEBUG
        std::cout << "last index was not file: " << tags << std::endl;
#endif
        return -1;
    }

    inodeFilenameMap.erase(fileInode);
    inodeTagMap.erase(fileInode);
    for (auto &tag: tags) {
        tagInodeMap[tag].erase(fileInode);
        if (tagInodeMap[tag].empty()) {
            tagNameTag.erase(tag.name);
            tagInodeMap.erase(tag);
        }
    }

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
    for (auto &tagName: tagNames) {
        if (tagNameTag.find(tagName) != tagNameTag.end()) {
            errno = EEXIST;
            return -1;
        }
    }
    for (auto &tagName: tagNames) {
        tag_t tag{TAG_TYPE_REGULAR, tagName};
        tagInodeMap[tag] = {};
        tagNameTag[tag.name] = tag;
    }
    return 0;
}

