#pragma once

#include <regex>
#include <optional>
#include <string>
#include <vector>

namespace app
{

struct SEMatch {
    int season;
    int episode;
    std::string title;
    std::string ext;
    std::vector<std::string> tags;
};

extern const std::vector<std::regex> SE_REGEXES;

// for a given string, find a match for the [title string, season number, episode number, extension string]
std::optional<SEMatch> find_se_match(const char *fn, const std::vector<std::regex>& se_regexes);
// for a given string, parse it so that it is more readable "This is an unclean string ?" --> "this.is.a.clean.string"
// cleaning the title involves an additional removal of contents within parenthesis to remove dates, which is fine for episode names
const std::string clean_se_name(const std::string &se_name);
const std::string clean_se_title(const std::string &se_name);

};