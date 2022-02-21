#include "file_loading.h"

#include <ostream>
#include <fstream>
#include <string>
#include <sstream>
#include <exception>

#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/reader.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace util {

rapidjson::SchemaDocument load_schema_from_cstr(const char *cstr) {
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
    rapidjson::ParseResult ok = doc.Parse(ss.str().c_str());
    if (!ok) {
        spdlog::error(fmt::format("JSON parse error: {} ({})", 
            rapidjson::GetParseError_En(ok.Code()), ok.Offset()));
        return {};
    }

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