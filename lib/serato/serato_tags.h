#ifndef SERATO_TAGS_H
#define SERATO_TAGS_H

#include <string>
#include <vector>
#include <map>

namespace serato {

class TagParser {
public:
    static std::map<std::string, std::string> readTags(const std::string& filePath);
};

} // namespace serato

#endif
