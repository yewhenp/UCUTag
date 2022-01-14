#ifndef UCUTAG_PROJECT_ARG_UTILS_H
#define UCUTAG_PROJECT_ARG_UTILS_H

#include <string>
#include <map>
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

std::map<std::string, std::string> parse_args(int argc, char **argv);

#endif //UCUTAG_PROJECT_ARG_UTILS_H
