#pragma once

#include <rapidjson/document.h>
#include <rapidjson/schema.h>

namespace app {

extern const char *CREDENTIALS_SCHEMA_STR;
extern rapidjson::SchemaDocument CREDENTIALS_SCHEMA;

}