#include "typedefs.h"

std::ostream& operator<<(std::ostream &os, const tag_t &tag) {
    os << "Tag(" << "type=" << tag.type << ", name=" << tag.name << ")";
    return os;
}
