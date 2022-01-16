#include <regex>
#include <optional>
#include <string>
#include <array>

#include "se_regex.h"
#include "Instrumentor.h"

namespace app {

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
            se.ext = mr[4].str();
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