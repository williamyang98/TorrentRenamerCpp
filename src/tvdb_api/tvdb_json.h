#pragma once
#include <vector>
#include <rapidjson/document.h>
#include "./tvdb_models.h"

namespace tvdb_api {

// load data from json documents
SeriesInfo load_series_info(const rapidjson::Document &doc);
EpisodesMap load_series_episodes_info(const rapidjson::Document &doc);
std::vector<SeriesInfo> load_search_info(const rapidjson::Document &doc);

}