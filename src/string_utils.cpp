#include "string_utils.h"


strvec split(const std::string &str, const std::string &delim) {
    size_t next, prev = 0;
    strvec result;
    while ((next = str.find(delim, prev)) != std::string::npos) {
        result.push_back(str.substr(prev, next - prev));
        prev = next + delim.length();
    }
    result.push_back(str.substr(prev));
    return result;
}