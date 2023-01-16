#include "app_folder_state.h"
#include "app_file_state.h"

#include <vector>
#include <filesystem>
#include <string>
#include <fmt/core.h>

namespace fs = std::filesystem;

namespace app 
{

AppFolderState::AppFolderState() 
: intents(), conflicts(), upcoming_rename_counts(), 
  is_conflict_table_dirty(false) 
{

}

AppFolderState::~AppFolderState() = default;

void AppFolderState::AddIntent(FileIntent&& file_intent) {
    is_conflict_table_dirty = true;
    auto intent = AppFileState(*this, std::move(file_intent));

    // Keep tracking of upcoming renames
    const bool is_rename = (intent.GetAction() == FileIntent::Action::RENAME);
    if (is_rename && intent.GetIsActive()) {
        auto& count = upcoming_rename_counts[intent.GetDest()];
        count++;
    }
    UpdateActionCount(intent.GetAction(), +1);

    intents.insert({intent.GetSrc(), std::move(intent)});
}

AppFolderState::ConflictTable& AppFolderState::GetConflicts() {
    UpdateConflictTable();
    return conflicts;
}

// if the conflict table has been labelled as dirty
// we can run this to bring it up to speed
void AppFolderState::UpdateConflictTable() {
    if (!is_conflict_table_dirty) {
        return;
    }

    conflicts.clear();

    for (auto& [key, intent]: intents) {
        const bool is_rename = (intent.GetAction() == FileIntent::Action::RENAME);

        // source conflicts with upcoming change, and it isn't a rename action
        if (!is_rename && (upcoming_rename_counts[intent.GetSrc()] > 0)) {
            intent.SetIsConflict(true);
            conflicts[intent.GetSrc()].push_back(key);
            continue;
        }

        // destination can't conflict if it isn't active
        if (!intent.GetIsActive()) {
            intent.SetIsConflict(false);
            continue;
        }

        // destination can't conflict if it isn't a rename
        if (!is_rename) {
            intent.SetIsConflict(false);
            continue;
        }

        // destination on rename conflicts with existing file or incoming change
        const bool is_dst_conflict =
            (upcoming_rename_counts[intent.GetDest()] > 1) ||
            (intents.find(intent.GetDest()) != intents.end());
        
        intent.SetIsConflict(is_dst_conflict);
        if (is_dst_conflict) {
            conflicts[intent.GetDest()].push_back(key);
            continue;
        }
    }

    is_conflict_table_dirty = false;
}

void AppFolderState::UpdateActionCount(FileIntent::Action action, int delta) {
    switch (action) {
    case FileIntent::Action::COMPLETE:
        action_counts.completes += delta; return;
    case FileIntent::Action::RENAME:
        action_counts.renames += delta; return;
    case FileIntent::Action::DELETE:
        action_counts.deletes += delta; return;
    case FileIntent::Action::IGNORE:
        action_counts.ignores += delta; return;
    case FileIntent::Action::WHITELIST:
        action_counts.whitelists += delta; return;
    default:
        return;
    }
}

}
