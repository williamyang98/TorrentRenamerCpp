#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/error/en.h>

#include <spdlog/spdlog.h>
#include <fmt/core.h>

#include "tvdb_api_schema.h"

namespace tvdb_api {

rapidjson::SchemaDocument load_schema_from_cstr(const char* cstr) {
    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(cstr);
    if (!ok) {
        spdlog::critical(fmt::format("JSON parse error: {} ({})\n", 
            rapidjson::GetParseError_En(ok.Code()), ok.Offset()));
        #ifndef NDEBUG
        assert(!ok.IsError());
        #else
        exit(EXIT_FAILURE);
        #endif
    }
    auto schema = rapidjson::SchemaDocument(doc);
    return schema;
}

const char* SEARCH_DATA_SCHEMA_STR =
R"({
    "title": "tvdb search data",
    "description": "TVDB search data",
    "type": "array",
    "items": {
        "type": "object",
        "properties": {
        "id": { "type": "number", "exclusiveMinimum": 0 },
        "seriesName": { "type": "string" },
        "firstAired": { "type": "string" },
        "status": { "type": "string" },
        "overview": { "type": "string" }
    },
    "required": ["id", "seriesName", "firstAired", "status"]
    }
})";

const char* SERIES_DATA_SCHEMA_STR =
R"({
    "title": "tvdb series data",
    "description": "TVDB series data",
    "type": "object",
    "properties": {
        "id": { "type": "number", "exclusiveMinimum": 0 },
        "seriesName": { "type": "string" },
        "firstAired": { "type": "string" },
        "status": { "type": "string" },
        "overview": { "type": "string" }
    },
    "required": ["id", "seriesName", "firstAired", "status"]
})";
  
const char* EPISODES_DATA_SCHEMA_STR =
R"({
    "title": "tvdb episodes data",
    "description": "An array of objects for each episode of a series",
    "type": "array",
    "items": {
        "type": "object",
        "properties": {
        "id": { "type": "number", "exclusiveMinimum": 0 },
        "airedSeason": { "type": "number", "exclusiveMinimum": 0 },
        "airedEpisodeNumber": { "type": "number", "exclusiveMinimum": 0 },
        "firstAired": { "type": ["string", "null"] },
        "episodeName": { "type": ["string", "null"] }
        },
        "required": ["id", "airedSeason", "airedEpisodeNumber", "firstAired", "episodeName"]
    }
})";

rapidjson::SchemaDocument SEARCH_DATA_SCHEMA = load_schema_from_cstr(SEARCH_DATA_SCHEMA_STR);
rapidjson::SchemaDocument SERIES_DATA_SCHEMA = load_schema_from_cstr(SERIES_DATA_SCHEMA_STR);
rapidjson::SchemaDocument EPISODES_DATA_SCHEMA = load_schema_from_cstr(EPISODES_DATA_SCHEMA_STR);
};