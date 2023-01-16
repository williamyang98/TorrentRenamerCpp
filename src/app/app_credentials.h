#pragma once
#include <string>
#include <optional>

namespace app
{

struct AppCredentials {
    std::string api_key;
    std::string user_key;
    std::string username;
    std::optional<std::string> token;
};

// THROWS: Runtime error with descriptor message
AppCredentials load_credentials_from_filepath(const char* filename);

};