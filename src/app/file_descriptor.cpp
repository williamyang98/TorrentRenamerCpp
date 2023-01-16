#include "file_descriptor.h"

#include <regex>
#include <optional>
#include <string>
#include <string>

#define TITLE_PATTERN  "([a-zA-Z\\.\\s\\-]*)[^a-zA-Z\\.\\s\\-]*"
#define EXT_PATTERN  "\\.([a-zA-Z0-9]+)"
const std::regex TAG_REGEX("[\\[\\(]([a-zA-Z0-9]{2,})[\\]\\)]");
const std::vector<std::regex> SEASON_EPISODE_EXT_REGEXES = {
    std::regex(TITLE_PATTERN "[Ss](\\d+)\\s*[Ee](\\d+)(.*)" EXT_PATTERN),
    std::regex(TITLE_PATTERN "[Ss]eason\\s*(\\d+)\\s*[Ee]pisode\\s*(\\d+)(.*)" EXT_PATTERN),
    std::regex(TITLE_PATTERN "(\\d+)\\s*x\\s*(\\d+)(.*)" EXT_PATTERN),
    std::regex(TITLE_PATTERN "[^\\w]+(\\d)(\\d\\d)[^\\w]+(.*)" EXT_PATTERN),
};

static std::vector<std::string> find_tags(const std::string& str_in);
static std::string trim(const std::string& s);

namespace app 
{

std::optional<FileDescriptor> find_descriptor(const std::string& filename) {
    for (auto& reg: SEASON_EPISODE_EXT_REGEXES) {
        std::match_results<std::string::const_iterator> mr;
        std::regex_search(filename, mr, reg);
        if (mr.size() == 0) continue;

        FileDescriptor se;
        se.title = mr[1].str();
        se.season = std::stoi(mr[2].str());
        se.episode = std::stoi(mr[3].str());
        se.tags = find_tags(mr[4].str());
        se.ext = mr[5].str();
        return se;
    }

    return {};
};

std::string clean_name(const std::string& name) {
    const std::regex remove_regex("[',\\(\\)\\[\\]]");
    const std::regex replace_regex("[^a-zA-Z0-9]+");

    std::string new_name = std::regex_replace(name, remove_regex, "");
    new_name = std::regex_replace(new_name, replace_regex, " ");
    new_name = trim(new_name);
    std::replace(new_name.begin(), new_name.end(), ' ', '.');
    return new_name;
}

std::string clean_title(const std::string& title) {
    const std::regex remove_regex("[',\\(\\)\\[\\]]");
    const std::regex remove_tags("[\\[\\(].*[\\)\\]]");
    const std::regex replace_regex("[^a-zA-Z0-9]+");

    std::string new_title = std::regex_replace(title, remove_tags, "");
    new_title = std::regex_replace(new_title, remove_regex, "");
    new_title = std::regex_replace(new_title, replace_regex, " ");
    new_title = trim(new_title);
    std::replace(new_title.begin(), new_title.end(), ' ', '.');
    return new_title;
}

};

std::vector<std::string> find_tags(const std::string& str_in) {
    std::vector<std::string> tags;
    std::smatch res;

    std::string str_proc = str_in;
    while (std::regex_search(str_proc, res, TAG_REGEX)) {
        // we take the second group since that contains our enclosed tag
        // the first group includes the surrounding brackets
        const auto& tag = res[1];
        tags.push_back(tag);
        str_proc = res.suffix();
    }

    return tags;
}

std::string trim(const std::string& s) {
    auto start = s.begin();
    while ((start != s.end()) && std::isspace(*start)) {
        start++;
    }
 
    auto end = s.end();
    do {
        end--;
    } while ((std::distance(start, end) > 0) && std::isspace(*end));
 
    return std::string(start, end + 1);
}