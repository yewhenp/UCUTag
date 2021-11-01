#ifndef UCUTAG_PROJECT_TYPEDEFS_H
#define UCUTAG_PROJECT_TYPEDEFS_H


#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <ostream>
#include <iterator>

typedef struct tag_t {
    std::size_t id = 0;
    std::size_t type = 0;
    std::string name{};
} tag_t;

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ","));
    return os;
}


std::ostream& operator<<(std::ostream &os, const tag_t &tag) {
    os << "Tag(id=" << tag.id << ", type=" << tag.type << ", name=" << tag.name << ")";
    return os;
}

// essential data structures for fs
typedef std::unordered_map<std::size_t, std::unordered_set<tag_t>> inodeTagMap_t;
typedef std::unordered_map<tag_t, std::unordered_set<std::size_t>> tagInodeMap_t;
typedef std::unordered_map<std::size_t, std::string> inodeFilenameMap_t;
typedef std::unordered_map<std::string, tag_t> tagNameTag_t;

// for convenience
typedef std::unordered_set<tag_t> tagsset;
typedef std::vector<tag_t> tagsvec;
typedef std::vector<std::string> strvec;

#endif //UCUTAG_PROJECT_TYPEDEFS_H
