#pragma once

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

namespace tvdb_api {

extern const char *SEARCH_DATA_SCHEMA_STR;
extern const char *SERIES_DATA_SCHEMA_STR;
extern const char *EPISODES_DATA_SCHEMA_STR;

extern rapidjson::SchemaDocument SEARCH_DATA_SCHEMA;
extern rapidjson::SchemaDocument SERIES_DATA_SCHEMA;
extern rapidjson::SchemaDocument EPISODES_DATA_SCHEMA;


}