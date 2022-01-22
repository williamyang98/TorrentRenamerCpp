#include <regex>
#include <string>
#include <array>
#include <optional>

#include "se_regex.h"
#include "Instrumentor.h"

namespace app {

#define TITLE_PATTERN  "([a-zA-Z\\.\\s\\-]*)[^a-zA-Z\\.\\s\\-]*"
#define EXT_PATTERN  "\\.([a-zA-Z0-9]+)"
#define TOTAL_PATTERNS 4

const std::regex TAG_REGEX("[\\[\\(]([a-zA-Z0-9]{2,})[\\]\\)]");

constexpr std::array<const char *, TOTAL_PATTERNS> SE_PATTERNS {
    TITLE_PATTERN "[Ss](\\d+)\\s*[Ee](\\d+)(.*)" EXT_PATTERN,
    TITLE_PATTERN "[Ss]eason\\s*(\\d+)\\s*[Ee]pisode\\s*(\\d+)(.*)" EXT_PATTERN,
    TITLE_PATTERN "(\\d+)\\s*x\\s*(\\d+)(.*)" EXT_PATTERN,
    TITLE_PATTERN "[^\\w]+(\\d)(\\d\\d)[^\\w]+(.*)" EXT_PATTERN,
};

extern const std::vector<std::regex> SE_REGEXES = []() {
    std::vector<std::regex> regexes;
    for (const auto &pattern: SE_PATTERNS) {
        regexes.push_back(std::regex(pattern));
    }
    return regexes;
} ();

static std::vector<std::string> find_tags(const std::string &str_in) {
    std::vector<std::string> tags;
    std::smatch res;

    std::string str_proc = str_in;
    while (std::regex_search(str_proc, res, TAG_REGEX)) {
        // we take the second group since that contains our enclosed tag
        // the first group includes the surrounding brackets
        const auto &tag = res[1];
        tags.push_back(tag);
        str_proc = res.suffix();
    }

    return tags;
}

std::optional<SEMatch> find_se_match(const char *fn, const std::vector<std::regex>& se_regexes) {
    PROFILE_FUNC();

    auto fn_str = std::basic_string(fn);

    for (auto &reg: se_regexes) {
        PROFILE_SCOPE("find_se_match::loop");
        std::match_results<std::string::const_iterator> mr;

        {
            PROFILE_SCOPE("find_se_match::loop::regex");
            std::regex_search(fn_str, mr, reg);
        }

        if (mr.size()) {
            PROFILE_SCOPE("find_se_match::loop::return");
            SEMatch se;
            se.title = mr[1].str();
            se.season = std::stoi(mr[2].str());
            se.episode = std::stoi(mr[3].str());
            se.ext = mr[5].str();
            se.tags = find_tags(mr[4].str());

            return se;
        }
    }

    return {};
};

static std::string trim(const std::string &s)
{
    PROFILE_FUNC();

    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) {
        start++;
    }
 
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
 
    return std::string(start, end + 1);
}

const std::string clean_se_name(const std::string &se_name) {
    PROFILE_FUNC();

    const std::regex remove_regex("[',\\(\\)\\[\\]]");
    const std::regex replace_regex("[^a-zA-Z0-9]+");

    std::string cleaned_string = std::regex_replace(se_name, remove_regex, "");
    cleaned_string = std::regex_replace(cleaned_string, replace_regex, " ");
    cleaned_string = trim(cleaned_string);
    std::replace(cleaned_string.begin(), cleaned_string.end(), ' ', '.');
    return cleaned_string;
}

const std::string clean_se_title(const std::string &se_name) {
    PROFILE_FUNC();

    const std::regex remove_regex("[',\\(\\)\\[\\]]");
    const std::regex remove_tags("[\\[\\(].*[\\)\\]]");
    const std::regex replace_regex("[^a-zA-Z0-9]+");

    std::string cleaned_string = std::regex_replace(se_name, remove_tags, "");
    cleaned_string = std::regex_replace(cleaned_string, remove_regex, "");
    cleaned_string = std::regex_replace(cleaned_string, replace_regex, " ");
    cleaned_string = trim(cleaned_string);
    std::replace(cleaned_string.begin(), cleaned_string.end(), ' ', '.');
    return cleaned_string;
}

};