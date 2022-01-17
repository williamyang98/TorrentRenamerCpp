#include <vector>
#include <filesystem>
#include <map>
#include <string>
#include <fmt/core.h>
#include <chrono>
#include <unordered_set>

#include "Instrumentor.h"
#include "console_colours.h"

#include "se_regex.h"
#include "scanner.h"
#include "file_loading.h"

namespace app {

namespace fs = std::filesystem;

void SeriesState::UpdateActionCount(FileIntent::Action action, int delta) {
    switch (action) {
    case FileIntent::Action::COMPLETE:
        action_counts.completes++; return;
    case FileIntent::Action::RENAME:
        action_counts.renames++; return;
    case FileIntent::Action::DELETE:
        action_counts.deletes++; return;
    case FileIntent::Action::IGNORE:
        action_counts.ignores++; return;
    default:
        return;
    }
}

void SeriesState::AddIntent(FileIntent &intent) {
    is_conflict_table_dirty = true;
    intents[intent.src] = intent; 
    if (intent.action == FileIntent::Action::RENAME) {
        upcoming_counts[intent.dest]++;
    }
    UpdateActionCount(intent.action, +1);
}

void SeriesState::OnIntentUpdate(FileIntent &intent, FileIntent::Action new_action) {
    if (intent.action == new_action) {
        return;
    }

    UpdateActionCount(intent.action, -1);
    UpdateActionCount(new_action, +1);

    is_conflict_table_dirty = true;

    const auto old_action = intent.action;
    const auto rename_action = FileIntent::Action::RENAME;
    intent.action = new_action;

    // update upcoming counts
    if (old_action != rename_action && new_action == rename_action) {
        upcoming_counts[intent.dest]++;
    } else if (old_action == rename_action && new_action != rename_action) {
        upcoming_counts[intent.dest]--;
    } 
}

void SeriesState::OnIntentUpdate(FileIntent &intent, bool new_is_active) {
    // TODO: imgui checkbox fix required
    // if (intent.is_active == new_is_active) {
    //     return;
    // }

    intent.is_active = new_is_active;

    if (intent.action != FileIntent::Action::RENAME) {
        return;
    }

    is_conflict_table_dirty = true;

    // update upcoming counts
    if (new_is_active) {
        upcoming_counts[intent.dest]++;
    } else {
        upcoming_counts[intent.dest]--;
    } 
}

void SeriesState::UpdateConflictTable() {
    if (!is_conflict_table_dirty) {
        return;
    }

    // clear the conflicts table
    conflicts.clear();

    for (auto &[key, intent]: intents) {
        // source conflicts with upcoming change
        if (upcoming_counts[intent.src] > 0) {
            intent.is_conflict = true;
            conflicts[intent.src].push_back(key);
            continue;
        }

        // a rename destination will conflict if it is active
        bool might_conflict = (intent.action == FileIntent::Action::RENAME) && intent.is_active;
        if (!might_conflict) {
            intent.is_conflict = false;
            continue;
        }

        // destination on rename conflicts with existing file or incoming change
        // and it is also active
        bool is_dst_conflict =
            (upcoming_counts[intent.dest] > 1) ||
            (intents.find(intent.dest) != intents.end());
        
        intent.is_conflict = is_dst_conflict;
        
        if (is_dst_conflict) {
            conflicts[intent.dest].push_back(key);
            continue;
        }
    }

    is_conflict_table_dirty = false;
}

SeriesState scan_directory(
    const fs::path &root, 
    const TVDB_Cache &cache,
    const RenamingConfig &cfg) 
{
    PROFILE_FUNC();

    SeriesState ss;

    if (!fs::is_directory(root)) {
        return ss;
    }

    // get required actions
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

        // std::cout << std::endl << "* Processing " << old_rel_path << std::endl;
        // std::cout << "filename=" << filename << std::endl;

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