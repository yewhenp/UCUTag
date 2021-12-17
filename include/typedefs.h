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
typedef ssize_t num_t;

typedef struct tag_t {
    num_t type = TAG_TYPE_REGULAR;
    std::string name{};

    bool operator==(const tag_t &other) const {
        return (type == other.type && name == other.name);
    }
} tag_t;

template<>
struct std::hash<tag_t> {
    std::size_t operator()(const tag_t &k) const {
        return ((hash<string>()(k.name)) ^ (hash<std::size_t>()(k.type) << 1));
    }
};


enum { FILE_NAME, TAG_NAME, TIME} tagType;

template <class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ","));
    return os;
}

template <class A, class B>
std::ostream& operator<<(std::ostream& os, const std::unordered_map<A, B>& m) {
    os << "Printing map: " << "\n";
    for(const auto& elem : m) {
        os << elem.first << ": " << elem.second <<  "\n";
    }
    return os;
}

template <class A>
std::ostream& operator<<(std::ostream& os, const std::unordered_set<A>& s) {
    os << "Printing set: " << "\n";
    for(const auto& elem : s) {
        os << elem <<  "\n";
    }
    return os;
}


std::ostream& operator<<(std::ostream &os, const tag_t &tag);

// for convenience
typedef ssize_t num_t;
typedef std::unordered_set<tag_t> tagset;
typedef std::vector<tag_t> tagvec;
typedef std::vector<std::string> strvec;
typedef std::unordered_set<num_t> inodeset;
typedef std::vector<num_t> numvec;

// essential data structures for fs
typedef std::unordered_map<std::size_t, tagset> inodeTagMap_t;
typedef std::unordered_map<tag_t, inodeset> tagInodeMap_t;
typedef std::unordered_map<std::size_t, std::string> inodeFilenameMap_t;
typedef std::unordered_map<std::string, tag_t> tagNameTag_t;


#endif //UCUTAG_PROJECT_TYPEDEFS_H
