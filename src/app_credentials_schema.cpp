#include "app_credentials_schema.h"

namespace app {

extern const char *CREDENTIALS_SCHEMA_STR =
R"({
    'title': 'tvdb credentials',
    'description': 'TVDB credentials file',
    'type': 'object',
    'properties': {
        'credentials': {
        'type': 'object',
        'properties': {
            'apikey': { 'type': 'string' },
            'username': { 'type': 'string' },
            'userkey': { 'type': 'string' }
        },
        'required': ['apikey', 'username', 'userkey']
        },
        'token': {
        'type': 'string'
        }
    },
    'required': ['credentials']
})";

extern rapidjson::SchemaDocument CREDENTIALS_SCHEMA = []() {
    rapidjson::Document doc;
    doc.Parse(CREDENTIALS_SCHEMA_STR);
    return rapidjson::SchemaDocument(doc);
} ();

}