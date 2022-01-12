#ifndef UCUTAG_PROJECT_STRING_UTILS_H
#define UCUTAG_PROJECT_STRING_UTILS_H

#include <vector>
#include <string>
#include "typedefs.h"

strvec split(const std::string &str, const std::string &delim);
void fillTagStat(struct stat *stbuf);
// const std::hash<std::string> hasher;

#endif //UCUTAG_PROJECT_STRING_UTILS_H
