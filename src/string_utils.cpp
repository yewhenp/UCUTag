#include "string_utils.h"
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>

strvec split(const std::string &str, const std::string &delim) {
    size_t next, prev = 0;
    strvec result;
    while ((next = str.find(delim, prev)) != std::string::npos) {
        std::string push_str = str.substr(prev, next - prev);
        if (!push_str.empty())
            result.push_back(push_str);
        prev = next + delim.length();
    }
    if (!str.substr(prev).empty())
        result.push_back(str.substr(prev));
    return result;
}

void fillTagStat(struct stat *stbuf) {
    stbuf->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
    stbuf->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
    stbuf->st_atime = time( nullptr ); // The last "a"ccess of the file/directory is right now
    stbuf->st_mtime = time( nullptr ); // The last "m"odification of the file/directory is right now
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
}


