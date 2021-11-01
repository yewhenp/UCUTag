#include "TagFS.h"

tagsvec TagFS::parse_tags(char *path) {
    tagsvec res;
    auto spath = std::string(path);
    auto splitted = split(spath, "/");
    std::transform(splitted.begin(), splitted.end(), std::back_inserter(res),
                   [this](const std::string &name) -> tag_t {return tagNameTag[name];});
#ifdef DEBUG
    std::cout << "Parsed path: " << path << ": " << res << std::endl;
#endif
    return res;
}
