#include "renamer.h"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

namespace app 
{

namespace fs = std::filesystem;

template <typename T>
bool rename_series_directory(const fs::path &root, T &intents) {
    bool all_success = true;

    for (FileIntent &intent: intents) {
        if (!intent.is_active) {
            continue;
        }

        // allow deletes to occur even if they are conflicting
        // this is fine since deleting a conflicting source file could resolve conflicts
        if (intent.action == FileIntent::Action::DELETE) {
            auto src_path = root / intent.src;
            try {
                fs::remove(src_path);
            } catch (fs::filesystem_error &ex) {
                all_success = false;
                spdlog::warn(fmt::format("Failed to remove file ({}): {}", src_path.string(), ex.what()));
            }
            continue;
        }

        // if intent is conflicting, stop it from overriding files
        if (intent.is_conflict) {
            continue;
        }

        if (intent.action == FileIntent::Action::RENAME) {
            auto src_path = root / intent.src;
            auto dest_path = root / intent.dest;
            try {
                auto dest_path_folder = fs::path(dest_path).remove_filename();
                fs::create_directories(dest_path_folder);
                fs::rename(src_path, dest_path);
            } catch (fs::filesystem_error &ex) {
                all_success = false;
                spdlog::warn(fmt::format(
                    "Failed to rename pending file ({}) to ({}): {}", 
                    src_path.string(), dest_path.string(), ex.what()));
            } 
            continue;
        }

    }

    return all_success;
}

};
