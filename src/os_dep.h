#pragma once

#include <string>
#include <optional>

namespace os_dep {

std::optional<std::string> open_folder_dialog(void);
void open_folder(const std::string& path);
void open_file(const std::string& path);


};
