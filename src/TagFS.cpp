#include "TagFS.h"

tagvec TagFS::parse_tags(const char *path) {
    tagvec res;
    auto spath = std::string(path);
    auto splitted = split(spath, "/");
    std::transform(splitted.begin(), splitted.end(), std::back_inserter(res),
                   [this](const std::string &name) -> tag_t {return tagNameTag[name];});
#ifdef DEBUG
//    std::cout << "Parsed path: " << path << ": " << res << std::endl;
#endif
    return res;
}

inode TagFS::get_file_inode(tagvec& tags) {
    inodeset file_inode_set = get_tag_set(tags);
    if (file_inode_set.size() > 1) {
#ifdef DEBUG
        std::cerr << "got multiple inodes in file tags: " << tags << std::endl;
#endif
        return static_cast<size_t>(-1);
    }
    if (file_inode_set.empty()) {
#ifdef DEBUG
        std::cout << "got empty inode set in file tags: " << tags << std::endl;
#endif
        return static_cast<size_t>(-1);
    }
    inode file_inode = *file_inode_set.begin();
    return file_inode;
}


std::string TagFS::get_file_real_path(tagvec& tags) {
    inode file_inode = get_file_inode(tags);
    return std::to_string(file_inode);
}

inodeset TagFS::get_tag_set(tagvec &tags) {
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

inode TagFS::get_new_inode() const {
    return inodeFilenameMap.size();
}

int TagFS::create_new_file(tagvec &tags, inode new_inode) {
    tags[tags.size() - 1].type = TAG_TYPE_FILE;
    tagset set(tags.begin(), tags.end());

    if (tagInodeMap.find(tags[tags.size() - 1]) != tagInodeMap.end()) {
#ifdef DEBUG
        std::cout << "file exist: " << tags << std::endl;
#endif
        return -1;
    }

    inodeFilenameMap[new_inode] = tags[tags.size() - 1].name;
    inodeTagMap[new_inode] = set;
    for (auto &tag: tags) {
        tagInodeMap[tag].insert(new_inode);
        tagNameTag[tag.name] = tag;
    }
    return 0;
}

inodeset TagFS::select(const char *path, bool cache) {
    tagvec tags = tagFS.parse_tags(path);
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

int TagFS::delete_file(tagvec &tags, inode file_inode) {
    if (tags[tags.size() - 1].type != TAG_TYPE_FILE) {
#ifdef DEBUG
        std::cout << "last index was not file: " << tags << std::endl;
#endif
        return -1;
    }

    inodeFilenameMap.erase(file_inode);
    inodeTagMap.erase(file_inode);
    for (auto &tag: tags) {
        tagInodeMap[tag].erase(file_inode);
        if (tagInodeMap[tag].empty()) {
            tagNameTag.erase(tag.name);
            tagInodeMap.erase(tag);
        }
    }

    return 0;
}

