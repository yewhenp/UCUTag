#ifndef UCUTAG_PROJECT_STRING_UTILS_H
#define UCUTAG_PROJECT_STRING_UTILS_H

#include <vector>
#include <string>
#include "typedefs.h"

strvec split(std::string &str, const std::string &delim) {
    size_t next, prev = 0;
    strvec result;
    while ((next = str.find(delim, prev)) != std::string::npos) {
        result.push_back(str.substr(prev, next - prev));
        prev = next + delim.length();
    }
    result.push_back(str.substr(prev));
    return result;
}

#endif //UCUTAG_PROJECT_STRING_UTILS_H
