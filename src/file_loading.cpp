#include <ostream>
#include <fstream>
#include <string>
#include <sstream>

#include <unordered_map>
#include <array>

#include <exception>

#include <rapidjson/reader.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "file_loading.h"

namespace app {

// for all loading checks, refer to the schema file (schema.json) for structure of document
SeriesInfo load_series_info(const rapidjson::Document &doc) {
    auto get_string_default = [](const rapidjson::Value &v) {
        return v.IsString() ? v.GetString() : "";
    };

    SeriesInfo series;
    series.id = doc["id"].GetUint();
    series.name = doc["seriesName"].GetString();
    series.air_date = get_string_default(doc["firstAired"]);
    series.status = get_string_default(doc["status"]);
    return series;
}

EpisodesMap load_series_episodes_info(const rapidjson::Document &doc) {
    auto episodes = EpisodesMap();
    auto &data = doc.GetArray();

    auto get_string_default = [](const rapidjson::Value &v) {
        return v.IsString() ? v.GetString() : "";
    };

    for (auto &e: data) {
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


std::vector<SeriesInfo> load_search_info(const rapidjson::Document &doc) {
    std::vector<SeriesInfo> series;

    auto data = doc.GetArray();
    series.reserve(data.Size());

    for (auto &s: data) {
        SeriesInfo o;
        o.name = s["seriesName"].GetString();
        o.air_date = s["firstAired"].GetString();
        o.status = s["status"].GetString();
        o.id = s["id"].GetUint();
        series.push_back(o);
    }
    
    return series;
}

app_schema_t load_app_schema_from_buffer(const char *data) {
    app_schema_t schema;

    rapidjson::Document doc;
    doc.Parse(data);

    static std::array<const char *, 4> REQUIRED_KEYS = 
        {SCHEME_CRED_KEY, SCHEME_SERIES_KEY, SCHEME_EPISODES_KEY, SCHEME_SEARCH_KEY};
    
    for (const auto &key: REQUIRED_KEYS) {
        if (!doc.HasMember(key)) {
            throw std::runtime_error(fmt::format("App schema missing key ({})", key));
        }

        rapidjson::Document sub_doc;
        sub_doc.CopyFrom(doc[key], doc.GetAllocator());
        schema.emplace(std::string(key), sub_doc);
    }

    return schema;
}

app_schema_t load_schema_from_file(const char *schema_fn) {
    std::ifstream file(schema_fn);
    if (!file.is_open()) {
        throw std::runtime_error(fmt::format("Schema file missing at filename=({})", schema_fn));
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    return app::load_app_schema_from_buffer(ss.str().c_str());
}

bool validate_document(const rapidjson::Document &doc, rapidjson::SchemaDocument &schema_doc) {
    rapidjson::SchemaValidator validator(schema_doc);
    if (!doc.Accept(validator)) {
        spdlog::error("Doc doesn't match schema");

        rapidjson::StringBuffer sb;
        validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
        spdlog::error(fmt::format("document pointer: {}", sb.GetString()));
        spdlog::error(fmt::format("error-type: {}", validator.GetInvalidSchemaKeyword()));
        sb.Clear();

        validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
        spdlog::error(fmt::format("schema pointer: {}", sb.GetString()));
        sb.Clear();

        return false;
    }

    return true;
}

DocumentLoadResult load_document_from_file(const char *fn) {
    DocumentLoadResult res;

    std::ifstream file(fn);
    if (!file.is_open()) {
        spdlog::warn(fmt::format("Unable to open file: {}", fn));
        res.code = DocumentLoadCode::FILE_NOT_FOUND;
        return res;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    rapidjson::Document doc;
    doc.Parse(ss.str().c_str());

    res.code = DocumentLoadCode::OK;
    res.doc = std::move(doc);
    return res;
}

void write_json_to_stream(const rapidjson::Document &doc, std::ostream &os) {
    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    writer.SetIndent(' ', 1);

    doc.Accept(writer);
    os << sb.GetString() << std::endl;
}

bool write_document_to_file(const char *fn, const rapidjson::Document &doc) {
    std::ofstream file(fn);
    if (!file.is_open()) {
        return false;
    }

    write_json_to_stream(doc, file);
    file.close();
    return true;
}

};