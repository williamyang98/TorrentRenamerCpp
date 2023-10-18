#include "app_schemas.h"
#include "util/file_loading.h"

namespace app
{

const char* APP_CONFIG_SCHEMA = 
R"({
    "title": "app config",
    "descripton": "Config file for gui app",
    "type": "object",
    "properties": {
        "credentials_file": {
            "type": "string"
        },
        "whitelist_folders": {
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "whitelist_filenames": {
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "blacklist_extensions": {
            "type": "array",
            "items": {
                "type": "string"
            }
        },
        "whitelist_tags": {
            "type": "array",
            "items": {
                "type": "string"
            }
        }
    },
    "required": ["credentials_file"]
})";

const char* CREDENTIALS_SCHEMA_STR =
R"({
    "title": "tvdb credentials",
    "description": "TVDB credentials file",
    "type": "object",
    "properties": {
        "credentials": {
        "type": "object",
        "properties": {
            "apikey": { "type": "string" },
            "username": { "type": "string" },
            "userkey": { "type": "string" }
        },
        "required": ["apikey", "username", "userkey"]
        },
        "token": {
        "type": "string"
        }
    },
    "required": ["credentials"]
})";

const char* APP_FOLDER_BOOKMARKS_SCHEMA_CSTR = 
R"({
    "title": "app folder bookmarks",
    "description": "Bookmarks for files in a folder",
    "type": "array",
    "items": {
        "type": "object",
        "properties": {
            "id": { "type": "string" },
            "is_favourite": { "type": "boolean" },
            "is_read": { "type": "boolean" },
            "is_unread": { "type": "boolean" }
        },
        "required": ["id"]
    }
})";

rapidjson::SchemaDocument APP_FOLDER_BOOKMARKS_SCHEMA_DOC = util::load_schema_from_cstr(APP_FOLDER_BOOKMARKS_SCHEMA_CSTR);
rapidjson::SchemaDocument APP_SCHEMA_DOC = util::load_schema_from_cstr(APP_CONFIG_SCHEMA);
rapidjson::SchemaDocument CREDENTIALS_SCHEMA = util::load_schema_from_cstr(CREDENTIALS_SCHEMA_STR);

};