#include "./app_folder_bookmarks_json.h"
#include "./app_folder_bookmarks.h"
#include "./app_schemas.h"
#include "util/expected.hpp"
#include "util/file_loading.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/schema.h>

template <typename T>
static const bool get_bool_default(const T& v, const char* key) {
    if (!v.HasMember(key)) {
        return false;
    }
    return v[key].IsBool() ? v[key].GetBool() : false;
}

namespace app 
{

tl::expected<AppFolderBookmarks, const char*> load_app_folder_bookmarks(const rapidjson::Document& doc) {
    if (!util::validate_document(doc, APP_FOLDER_BOOKMARKS_SCHEMA_DOC)) {
        return tl::make_unexpected<const char*>("Failed to validate folder bookmarks data");
    }

    AppFolderBookmarks bookmarks;

    auto data = doc.GetArray();
    for (auto& e: data) {
        const auto& id = e["id"].GetString();
        auto flags = AppFolderBookmarks::Flags{};
        flags.is_starred = get_bool_default(e, "is_favourite");
        flags.is_read = get_bool_default(e, "is_read");
        flags.is_unread = get_bool_default(e, "is_unread");
        bookmarks.m_flags.insert({ id, flags });
    }
    return bookmarks;
}

std::string json_stringify_app_folder_bookmarks(const AppFolderBookmarks& bookmarks) {
    rapidjson::StringBuffer sb;
    auto writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>(sb);
    writer.SetIndent(' ', 1);
    writer.StartArray();
    for (const auto& [filename, flags]: bookmarks.m_flags) {
        if (!flags.GetIsAnyFlagged()) {
            continue;
        }

        writer.StartObject();
        writer.Key("id");
        writer.String(filename.c_str());
        if (flags.is_starred) { writer.Key("is_favourite"); writer.Bool(flags.is_starred); }
        if (flags.is_unread) { writer.Key("is_unread"); writer.Bool(flags.is_unread); }
        if (flags.is_read) { writer.Key("is_read"); writer.Bool(flags.is_read); }
        writer.EndObject();
    }
    writer.EndArray();

    auto str = std::string(sb.GetString(), sb.GetSize());
    return str;
}

};

