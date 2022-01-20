#pragma once

#include <unordered_map>
#include <string>
#include <ostream>

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

namespace app
{

// stores info about series
struct SeriesInfo {
    uint32_t id;
    std::string name;
    std::string air_date;
    std::string status;
};

// stores info about an episode 
struct EpisodeInfo {
    uint32_t id;
    int season;
    int episode;
    std::string air_date;
    std::string name;

    EpisodeInfo() {
        id = 0;
        season = 0;
        episode = 0;
        air_date = "";
        name = "";
    }
};

// key for unordered map for episode lookup
struct EpisodeKey {
    const int season;
    const int episode;

    const int GetHash() const {
        return (season * (1 << 16)) + episode;
    }

    bool operator ==(const EpisodeKey &rhs) const {
        return GetHash() == rhs.GetHash();
    }
};

// define our hash function here to pass to unordered_map
struct EpisodeKeyHasher {
    std::size_t operator()(const app::EpisodeKey& k) const noexcept {
        return k.GetHash();
    }
};

typedef std::unordered_map<EpisodeKey, EpisodeInfo, EpisodeKeyHasher> EpisodesMap;

// a cache that stores combined series and episodes info
struct TVDB_Cache {
    SeriesInfo series;
    EpisodesMap episodes;
};

// load data from json documents
SeriesInfo load_series_info(const rapidjson::Document &doc);
EpisodesMap load_series_episodes_info(const rapidjson::Document &doc);
std::vector<SeriesInfo> load_search_info(const rapidjson::Document &doc);

// use validation schema and log any errors 
bool validate_document(const rapidjson::Document &doc, rapidjson::SchemaDocument &schema_doc);

// reading and writing json documents
enum DocumentLoadCode {
    OK, FILE_NOT_FOUND
};
struct DocumentLoadResult {
   DocumentLoadCode code;
   rapidjson::Document doc; 
};
DocumentLoadResult load_document_from_file(const char *fn);

void write_json_to_stream(const rapidjson::Document &doc, std::ostream &os);
bool write_document_to_file(const char *fn, const rapidjson::Document &doc);

};