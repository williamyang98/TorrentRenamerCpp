#pragma once

// helper file for json files
// - loads and writes a json file
// - load a schema file
// - validate a json document against a schema document

#include <ostream>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>

namespace util
{

// load schema from a buffer
rapidjson::SchemaDocument load_schema_from_cstr(const char *cstr);

// use validation schema and log any errors 
bool validate_document(const rapidjson::Document& doc, rapidjson::SchemaDocument& schema_doc);

// reading and writing json documents
enum DocumentLoadCode {
    OK, FILE_NOT_FOUND
};
struct DocumentLoadResult {
   DocumentLoadCode code;
   rapidjson::Document doc; 
};
DocumentLoadResult load_document_from_file(const char *fn);

void write_json_to_stream(const rapidjson::Document& doc, std::ostream& os);
bool write_document_to_file(const char *fn, const rapidjson::Document& doc);

};