#include "tvdb_api_schema.h"

namespace tvdb_api {

const char *SEARCH_DATA_SCHEMA_STR =
R"({
    'title': 'tvdb search data',
    'description': 'TVDB search data',
    'type': 'array',
    'items': {
        'type': 'object',
        'properties': {
        'id': { 'type': 'number', 'exclusiveMinimum': 0 },
        'seriesName': { 'type': 'string' },
        'firstAired': { 'type': 'string' },
        'status': { 'type': 'string' }
        },
        'required': ['id', 'seriesName', 'firstAired', 'status']
    }
})";

const char *SERIES_DATA_SCHEMA_STR =
R"({
    'title': 'tvdb series data',
    'description': 'TVDB series data',
    'type': 'object',
    'properties': {
        'id': { 'type': 'number', 'exclusiveMinimum': 0 },
        'seriesName': { 'type': 'string' },
        'firstAired': { 'type': 'string' },
        'status': { 'type': 'string' }
    },
    'required': ['id', 'seriesName', 'firstAired', 'status']
})";
  
const char *EPISODES_DATA_SCHEMA_STR =
R"({
    'title': 'tvdb episodes data',
    'description': 'An array of objects for each episode of a series',
    'type': 'array',
    'items': {
        'type': 'object',
        'properties': {
        'id': { 'type': 'number', 'exclusiveMinimum': 0 },
        'airedSeason': { 'type': 'number', 'exclusiveMinimum': 0 },
        'airedEpisodeNumber': { 'type': 'number', 'exclusiveMinimum': 0 },
        'firstAired': { 'type': ['string', 'null'] },
        'episodeName': { 'type': ['string', 'null'] }
        },
        'required': ['id', 'airedSeason', 'airedEpisodeNumber', 'firstAired', 'episodeName']
    }
})";

rapidjson::SchemaDocument SEARCH_DATA_SCHEMA = []() {
    rapidjson::Document doc;
    doc.Parse(SEARCH_DATA_SCHEMA_STR);
    return rapidjson::SchemaDocument(doc);
} ();

rapidjson::SchemaDocument SERIES_DATA_SCHEMA = []() {
    rapidjson::Document doc;
    doc.Parse(SERIES_DATA_SCHEMA_STR);
    return rapidjson::SchemaDocument(doc);
} ();

rapidjson::SchemaDocument EPISODES_DATA_SCHEMA = []() {
    rapidjson::Document doc;
    doc.Parse(EPISODES_DATA_SCHEMA_STR);
    return rapidjson::SchemaDocument(doc);
} ();

};