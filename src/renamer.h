#pragma once

#include <filesystem>
#include "folder_diff.h"

namespace app 
{

// give a set of actions, perform them on the contents of the root directory
template <typename T>
bool rename_series_directory(const std::filesystem::path &root, T &intents);

};

#include "renamer.cpp"