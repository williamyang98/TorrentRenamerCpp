#pragma once

#include <filesystem>
#include "folder_diff.h"

namespace app 
{

// give a set of actions, perform them on the contents of the root directory
bool rename_series_directory(const std::filesystem::path &root, FolderDiff &state);

};