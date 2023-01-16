#include "tvdb_json.h"
#include "tvdb_models.h"
#include <vector>
#include <rapidjson/document.h>

namespace tvdb_api {

// for all loading checks, refer to the schema file (schema.json) for structure of document
SeriesInfo load_series_info(const rapidjson::Document& doc) {
    auto get_string_default = [](const rapidjson::Value& v) {
        return v.IsString() ? v.GetString() : "";
    };

    SeriesInfo series;
    series.id = doc["id"].GetUint();
    series.name = doc["seriesName"].GetString();
    series.air_date = get_string_default(doc["firstAired"]);
    series.status = get_string_default(doc["status"]);
    return series;
}

EpisodesMap load_series_episodes_info(const rapidjson::Document& doc) {
    auto episodes = EpisodesMap();
    auto data = doc.GetArray();

    auto get_string_default = [](const rapidjson::Value& v) {
        return v.IsString() ? v.GetString() : "";
    };

    for (auto& e: data) {
        EpisodeInfo ep;
        ep.id = e["id"].GetUint();
        ep.season = e["airedSeason"].GetInt();
        ep.episode = e["airedEpisodeNumber"].GetInt();
        ep.air_date = get_string_default(e["firstAired"]);
        ep.name = get_string_default(e["episodeName"]);

        
        EpisodeKey key {ep.season, ep.episode};
        episodes[key] = ep;
    }

    return episodes;
}


std::vector<SeriesInfo> load_search_info(const rapidjson::Document& doc) {
    std::vector<SeriesInfo> series;

    auto data = doc.GetArray();
    series.reserve(data.Size());

    for (auto& s: data) {
        SeriesInfo o;
        o.name = s["seriesName"].GetString();
        o.air_date = s["firstAired"].GetString();
        o.status = s["status"].GetString();
        o.id = s["id"].GetUint();
        series.push_back(o);
    }
    
    return series;
}

}