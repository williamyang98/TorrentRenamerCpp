#pragma once

#include <regex>
#include <optional>
#include <string>
#include <array>

namespace app
{

struct SEMatch {
    int season;
    int episode;
    std::string title;
    std::string ext;
};

#define TITLE_PATTERN  "([a-zA-Z\\.\\s\\-]*)[^a-zA-Z\\.\\s\\-]*"
#define EXT_PATTERN  "\\.([a-zA-Z0-9]+)"
#define TOTAL_PATTERNS 4

constexpr std::array<const char *, TOTAL_PATTERNS> SE_PATTERNS {
    TITLE_PATTERN "[Ss](\\d+)\\s*[Ee](\\d+).*" EXT_PATTERN,
    TITLE_PATTERN "[Ss]eason\\s*(\\d+)\\s*[Ee]pisode\\s*(\\d+).*" EXT_PATTERN,
    TITLE_PATTERN "(\\d+)\\s*x\\s*(\\d+).*" EXT_PATTERN,
    TITLE_PATTERN "[^\\w]+(\\d)(\\d\\d)[^\\w]+.*" EXT_PATTERN,
};

const auto SE_REGEXES = 
    []() {
        std::vector<std::regex> regexes;
        for (const auto &pattern: SE_PATTERNS) {
            regexes.push_back(std::regex(pattern));
        }
        return regexes;
    } ();

std::optional<SEMatch> find_se_match(const char *fn, const std::vector<std::regex>& se_regexes);
const std::string clean_se_name(const std::string &se_name);
const std::string clean_se_title(const std::string &se_name);

};