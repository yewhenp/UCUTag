#ifndef UCUTAG_PROJECT_TYPEDEFS_H
#define UCUTAG_PROJECT_TYPEDEFS_H


#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <ostream>
#include <iterator>

#define TAG_TYPE_REGULAR 0
#define TAG_TYPE_FILE 1

typedef struct tag_t {
    std::size_t id = 0;
    std::size_t type = TAG_TYPE_REGULAR;
    std::string name{};

    bool operator==(const tag_t &other) const {
        return (id == other.id
              && type == other.type
              && name == other.name);
    }
} tag_t;

template<>
struct std::hash<tag_t> {
    std::size_t operator()(const tag_t &k) const {
        return ((hash<string>()(k.name) ^ (hash<std::size_t>()(k.id) << 1)) >> 1) ^ (hash<std::size_t>()(k.type) << 1);
    }
};


enum { FILE_NAME, TAG_NAME, TIME} tagType;

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ","));
    return os;
}


std::ostream& operator<<(std::ostream &os, const tag_t &tag);

// for convenience
typedef std::size_t inode;
typedef std::unordered_set<tag_t> tagset;
typedef std::vector<tag_t> tagvec;
typedef std::vector<std::string> strvec;
typedef std::unordered_set<inode> inodeset;
typedef std::vector<inode> inodevec;

// essential data structures for fs
typedef std::unordered_map<std::size_t, tagset> inodeTagMap_t;
typedef std::unordered_map<tag_t, inodeset> tagInodeMap_t;
typedef std::unordered_map<std::size_t, std::string> inodeFilenameMap_t;
typedef std::unordered_map<std::string, tag_t> tagNameTag_t;


#endif //UCUTAG_PROJECT_TYPEDEFS_H
