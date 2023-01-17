#include "file_intents.h"
#include "file_descriptor.h"
#include <filesystem>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace fs = std::filesystem;

static
std::string create_filename(
    const std::string& title, int season, int episode, 
    const std::string& name, const std::string& ext, 
    const std::vector<std::string>& tags);

namespace app 
{

FileIntent get_file_intent(
    const std::string& relative_path, 
    const FilterRules& rules, 
    const tvdb_api::TVDB_Cache& api_cache) 
{
    const auto fs_filepath = fs::path(relative_path);
    const auto fs_parent_path = fs_filepath.parent_path();
    const auto filename = fs_filepath.filename().string();
    const auto ext = fs_filepath.extension().string();

    FileIntent intent;
    intent.src = relative_path;
    intent.is_active = false;
    intent.is_conflict = false;

    for (auto& blacklist_ext: rules.blacklist_extensions) {
        if (ext.compare(blacklist_ext) == 0) {
            intent.action = FileIntent::Action::DELETE;
            return intent;
        }
    }

    for (auto& fs_folder: fs_parent_path) {
        auto folder = fs_folder.string();
        for (auto& whitelist_folder: rules.whitelist_folders) {
            if (folder.compare(whitelist_folder) == 0) {
                intent.action = FileIntent::Action::WHITELIST;
                return intent;
            }
        }
    }

    for (auto& whitelist_file: rules.whitelist_files) {
        if (filename.compare(whitelist_file) == 0) {
            intent.action = FileIntent::Action::WHITELIST;
            return intent;
        }
    }

    auto opt_descriptor = find_descriptor(filename);
    if (!opt_descriptor) {
        intent.action = FileIntent::Action::IGNORE;
        return intent;
    }

    // Try to rename file
    const auto& descriptor = opt_descriptor.value(); 
    const auto episode_key = tvdb_api::EpisodeKey{descriptor.season, descriptor.episode};

    // Get valid tags only
    // NOTE: Alot of torrent sources will have tags for the type of codec or language
    //       We are not interested in these miscellaneous tags
    std::vector<std::string> valid_tags;
    for (auto& tag: descriptor.tags) {
        for (auto& whitelist_tag: rules.whitelist_tags) {
            if (tag.compare(whitelist_tag) == 0) {
                valid_tags.push_back(tag);
                break;
            }
        }
    }

    // Get the new filepath
    const auto new_folder = fmt::format("Season {:02d}", episode_key.season);
    std::string new_filename;
    const auto& episodes_cache = api_cache.episodes;
    const auto& cache_result = episodes_cache.find(episode_key);
    if (cache_result != episodes_cache.end()) {
        auto& episode_data = cache_result->second;
        new_filename = create_filename(
            clean_title(api_cache.series.name),
            descriptor.season, descriptor.episode,
            clean_name(episode_data.name),
            descriptor.ext,
            valid_tags
        );
    } else {
        new_filename = create_filename(
            clean_title(api_cache.series.name),
            descriptor.season, descriptor.episode,
            "",
            descriptor.ext,
            valid_tags
        );
    }

    const auto fs_new_filepath = fs::path(new_folder) / fs::path(new_filename);
    const auto new_filepath = fs_new_filepath.string();
    const bool is_same_filepath = (fs_filepath == fs_new_filepath);

    if (is_same_filepath) {
        intent.action = FileIntent::Action::COMPLETE;
    } else {
        intent.action = FileIntent::Action::RENAME;
        intent.dest = new_filepath;
        intent.is_active = true;
    }

    return intent;
}

std::vector<FileIntent> get_directory_file_intents(
    const std::filesystem::path& root, 
    const FilterRules& rules, 
    const tvdb_api::TVDB_Cache& api_cache)
{
    auto intents = std::vector<FileIntent>();
    
    if (!fs::is_directory(root)) {
        return intents;
    }

    auto directory_iter = fs::recursive_directory_iterator(root);
    for (auto& entry: directory_iter) {
        if (!fs::is_regular_file(entry)) {
            continue;
        }

        const auto& fs_path = entry.path();
        const auto& fs_relative_path = fs_path.lexically_relative(root);
        const auto relative_path = fs_relative_path.string();
        auto intent = get_file_intent(relative_path, rules, api_cache);
        intents.push_back(intent);
    }

    return intents;
}

void execute_file_intent(const std::filesystem::path& root, const FileIntent& intent) {
    if (!intent.is_active) {
        return;
    }

    // NOTE: We don't care if we are deleting a file that has a filepath conflict
    //       Usually this means we are removing an old filepath 
    //       that is the same as an upcoming renamed filepath
    if (intent.action == FileIntent::Action::DELETE) {
        auto src_path = root / intent.src;
        fs::remove(src_path);
        return;
    }

    // NOTE: Prevent overriding of files in event of conflict
    if (intent.is_conflict) {
        return;
    }

    if (intent.action == FileIntent::Action::RENAME) {
        auto src_path = root / intent.src;
        auto dest_path = root / intent.dest;
        auto dest_path_folder = fs::path(dest_path).remove_filename();
        fs::create_directories(dest_path_folder);
        fs::rename(src_path, dest_path);
        return;
    }
}

};

std::string create_filename(
    const std::string& title, int season, int episode, 
    const std::string& name, const std::string& ext, 
    const std::vector<std::string>& tags)
{
    std::stringstream ss;
    ss << fmt::format("{}-S{:02d}E{:02d}", title, season, episode);
    if (name.size() > 0) {
        ss << "-" << name;
    }

    // create tag group
    bool created_prepend = false;
    for (auto& tag: tags) {
        // prepend delimiter if there are tags
        if (!created_prepend) {
            ss << ".";
            created_prepend = true;
        }
        ss << '[' << tag << ']';
    }
    ss << "." << ext;

    return ss.str();
}