#include <vector>
#include <filesystem>
#include <string>
#include <fmt/core.h>

#include "Instrumentor.h"
#include "console_colours.h"

#include "se_regex.h"
#include "scanner.h"
#include "file_loading.h"

namespace app {

namespace fs = std::filesystem;

FolderDiff scan_directory(
    const fs::path &root, 
    const TVDB_Cache &cache,
    const RenamingConfig &cfg) 
{
    PROFILE_FUNC();

    FolderDiff ss;

    if (!fs::is_directory(root)) {
        return ss;
    }

    // get required actions into folder diff
    auto &dir = fs::recursive_directory_iterator(root);
    for (auto &e: dir) {
        if (!fs::is_regular_file(e)) {
            continue;
        }


        PROFILE_SCOPE("File processing");

        const auto &path = e.path();
        const auto &old_rel_path = path.lexically_relative(root);
        const auto &filename_path = path.filename();

        const auto filename = filename_path.string();
        const auto old_rel_path_str = old_rel_path.string();

        // for each file determine action to perform, given cache
        const auto ext = filename_path.extension().string();

        FileIntent intent;
        intent.src = old_rel_path_str;
        intent.action = FileIntent::Action::IGNORE;
        intent.is_active = false;
        intent.is_conflict = false;

        // delete if blacklisted extension
        PROFILE_MANUAL_BEGIN(blacklist);
        bool is_invalid_ext = std::find_if(
            cfg.blacklist_extensions.begin(), 
            cfg.blacklist_extensions.end(), 
            [&ext](const std::string &invalid_ext) { 
                return ext.compare(invalid_ext) == 0;
            }) != cfg.blacklist_extensions.end();

        if (is_invalid_ext) {
            // std::cout << FMAG("[D] ") << old_rel_path << std::endl;
            intent.action = FileIntent::Action::DELETE;
            intent.is_active = false;
            ss.AddIntent(intent);
            continue;
        }
        PROFILE_MANUAL_END(blacklist);

        // ignore if in a special folder
        PROFILE_MANUAL_BEGIN(special_check);
        bool is_special_folder = std::find_if(
            cfg.special_folders.begin(),
            cfg.special_folders.end(),
            [&old_rel_path_str] (const std::string &folder) {
                return old_rel_path_str.compare(0, folder.size(), folder) == 0;
            }) != cfg.special_folders.end();
        
        if (is_special_folder) {
            // std::cout << FCYN("[I] ") << old_rel_path << std::endl;
            intent.action = FileIntent::Action::IGNORE;
            intent.is_active = false;
            ss.AddIntent(intent);
            continue;
        }
        PROFILE_MANUAL_END(special_check);

        // try to rename
        PROFILE_MANUAL_BEGIN(rename_block);

        auto mr = app::find_se_match(filename.c_str(), app::SE_REGEXES);
        // ignore if cant rename
        if (!mr) {
            // std::cout << FCYN("[I] ") << old_rel_path << std::endl;
            intent.action = FileIntent::Action::IGNORE;
            intent.is_active = false;
            ss.AddIntent(intent);
            continue;
        }

        PROFILE_MANUAL_END(rename_block);

        // if able to rename, it is a pending rename
        PROFILE_MANUAL_BEGIN(pending_block);

        const auto &match = mr.value();
        EpisodeKey key { match.season, match.episode };

        const auto &ep_table = cache.episodes;
        const auto new_parent_dir = root / fmt::format("Season {:02d}", key.season);

        std::string dest_fn;
        // if unable to retrieve tvdb data, give generic rename
        if (ep_table.find(key) == ep_table.end()) {
            PROFILE_SCOPE("pending_block::no_metadata");
            dest_fn = fmt::format(
                "{}-S{:02d}E{:02d}.{}", 
                app::clean_se_title(cache.series.name),
                match.season,
                match.episode,
                match.ext
            );
        // rename according to the metadata
        } else {
            PROFILE_SCOPE("pending_block::metadata");
            auto &ep = ep_table.at(key);
            dest_fn = fmt::format(
                "{}-S{:02d}E{:02d}-{}.{}", 
                app::clean_se_title(cache.series.name),
                match.season,
                match.episode,
                app::clean_se_name(ep.name),
                match.ext
            );
        }
        PROFILE_MANUAL_END(pending_block);

        PROFILE_MANUAL_BEGIN(complete_check);
        auto new_path = (new_parent_dir / dest_fn);
        auto new_rel_path = new_path.lexically_relative(root);
        auto new_rel_path_str = new_rel_path.string();

        if (old_rel_path == new_rel_path) {
            // std::cout << FGRN("[C] ") << old_rel_path_str << std::endl;
            intent.action = FileIntent::Action::COMPLETE;
            intent.is_active = false;
            ss.AddIntent(intent);
        } else {
            // std::cout << FYEL("[R] ") << old_rel_path_str << " ==> "  << new_rel_path_str << std::endl;
            intent.action = FileIntent::Action::RENAME;
            intent.dest = new_rel_path_str;
            intent.is_active = true;
            ss.AddIntent(intent);
        }
        PROFILE_MANUAL_END(complete_check);
    }

    return ss;
}

};