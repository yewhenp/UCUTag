#include "typedefs.h"

std::ostream& operator<<(std::ostream &os, const tag_t &tag) {
    os << "Tag(id=" << tag.id << ", type=" << tag.type << ", name=" << tag.name << ")";
    return os;
}
