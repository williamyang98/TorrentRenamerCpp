#pragma once

#include <optional>
#include <string>
#include <vector>

namespace app
{

// Descriptor associated with a filename of a season/episode file
struct FileDescriptor {
    int season;
    int episode;
    std::string title;
    std::string ext;
    std::vector<std::string> tags;
};

std::optional<FileDescriptor> find_descriptor(const std::string& filename);
std::string clean_name(const std::string& name);
std::string clean_title(const std::string& title);

};