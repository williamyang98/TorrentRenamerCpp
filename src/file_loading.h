#pragma once

#include <unordered_map>
#include <string>
#include <ostream>

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

#define SCHEME_CRED_KEY     "credentials_file" 
#define SCHEME_SERIES_KEY   "series_data" 
#define SCHEME_EPISODES_KEY "episodes_data"

namespace app
{

typedef std::unordered_map<std::string, rapidjson::SchemaDocument> app_schema_t;

struct SeriesInfo {
    uint32_t id;
    std::string name;
    std::string air_date;
    std::string status;
};

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

// hash key for unordered map for episode lookup
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

struct EpisodeKeyHasher {
    std::size_t operator()(const app::EpisodeKey& k) const noexcept {
        return k.GetHash();
    }
};

typedef std::unordered_map<EpisodeKey, EpisodeInfo, EpisodeKeyHasher> EpisodesMap;

struct TVDB_Cache {
    SeriesInfo series;
    EpisodesMap episodes;
};

SeriesInfo load_series_info(const rapidjson::Document &doc);
EpisodesMap load_series_episodes_info(const rapidjson::Document &doc);

app_schema_t load_app_schema_from_buffer(const char *data);
app_schema_t load_schema_from_file(const char *schema_fn);
void validate_document(const rapidjson::Document &doc, rapidjson::SchemaDocument &schema_doc, const char *label);

rapidjson::Document load_document_from_file(const char *fn);
void write_json_to_stream(const rapidjson::Document &doc, std::ostream &os);
void write_document_to_file(const char *fn, const rapidjson::Document &doc);

};