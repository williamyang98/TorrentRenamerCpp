#pragma once

#include <rapidjson/document.h>
#include "util/expected.hpp"
#include "./app_folder_bookmarks.h"

namespace app 
{

tl::expected<AppFolderBookmarks, const char*> load_app_folder_bookmarks(const rapidjson::Document& doc);

std::string json_stringify_app_folder_bookmarks(const AppFolderBookmarks& bookmarks);

}
