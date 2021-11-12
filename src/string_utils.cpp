#include "string_utils.h"


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
