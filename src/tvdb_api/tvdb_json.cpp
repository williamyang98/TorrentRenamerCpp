#include "tvdb_json.h"
#include "tvdb_models.h"
#include <vector>
#include <rapidjson/document.h>

#include "tvdb_api_schema.h"
#include "util/file_loading.h"

static 
const char* get_string_default(const rapidjson::Value& v) {
    return v.IsString() ? v.GetString() : "";
};

namespace tvdb_api 
{

// NOTE: Refer to tvdb_api_schema.cpp for schemas
tl::expected<SeriesInfo, const char*> load_series_info(const rapidjson::Document& doc) {
    if (!util::validate_document(doc, tvdb_api::SERIES_DATA_SCHEMA)) {
        return tl::make_unexpected<const char*>("Failed to validate series data");
    }

    SeriesInfo series;
    series.id = doc["id"].GetUint();
    series.name = doc["seriesName"].GetString();
    series.air_date = get_string_default(doc["firstAired"]);
    series.status = get_string_default(doc["status"]);

    if (doc.HasMember("overview")) {
        if (doc["overview"].IsString()) {
            series.overview = doc["overview"].GetString();
        } else {
            series.overview = std::nullopt;
        }
    }

    return series;
}

// NOTE: Refer to tvdb_api_schema.cpp for schemas
tl::expected<EpisodesMap, const char*> load_series_episodes_info(const rapidjson::Document& doc) {
    if (!util::validate_document(doc, tvdb_api::EPISODES_DATA_SCHEMA)) {
        return tl::make_unexpected<const char*>("Failed to validate episodes data");
    }

    auto episodes = EpisodesMap();
    auto data = doc.GetArray();

    for (auto& e: data) {
        EpisodeInfo ep;
        ep.id = e["id"].GetUint();
        ep.season = e["airedSeason"].GetInt();
        ep.episode = e["airedEpisodeNumber"].GetInt();
        ep.air_date = get_string_default(e["firstAired"]);
        ep.name = get_string_default(e["episodeName"]);
        if (e.HasMember("overview")) {
            ep.overview = e["overview"].IsString() ? e["overview"].GetString() : "";
        }
        EpisodeKey key {ep.season, ep.episode};
        episodes[key] = ep;
    }

    return episodes;
}

// NOTE: Refer to tvdb_api_schema.cpp for schemas
tl::expected<std::vector<SeriesInfo>, const char*> load_search_info(const rapidjson::Document& doc) {
    if (!util::validate_document(doc, tvdb_api::SEARCH_DATA_SCHEMA)) {
        return tl::make_unexpected<const char*>("Failed to validate series search data");
    }

    auto series = std::vector<SeriesInfo>();
    auto data = doc.GetArray();
    series.reserve(data.Size());

    for (auto& s: data) {
        auto& o = series.emplace_back();
        o.name = s["seriesName"].GetString();
        o.air_date = s["firstAired"].GetString();
        o.status = s["status"].GetString();
        o.id = s["id"].GetUint();
    }
    
    return series;
}

}