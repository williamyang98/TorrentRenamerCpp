#pragma once

#include <filesystem>
#include "scanner.h"

namespace app 
{

bool rename_series_directory(const std::filesystem::path &root, SeriesState &state);

};