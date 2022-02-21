#pragma once

#include <filesystem>

namespace app 
{

// give a set of actions, perform them on the contents of the root directory
template <typename T>
bool rename_series_directory(const std::filesystem::path &root, T &intents);

};

// we need template definition
#include "renamer.cpp"