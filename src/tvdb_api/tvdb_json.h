#pragma once

#include <string>
#include <vector>
#include <rapidjson/document.h>
#include "./tvdb_models.h"
#include "util/expected.hpp"

namespace tvdb_api {

// load data from json documents
tl::expected<SeriesInfo, const char*> load_series_info(const rapidjson::Document& doc);
tl::expected<EpisodesMap, const char*> load_series_episodes_info(const rapidjson::Document& doc);
tl::expected<std::vector<SeriesInfo>, const char*> load_search_info(const rapidjson::Document& doc);

}