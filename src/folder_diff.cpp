#include <vector>
#include <filesystem>
#include <string>
#include <fmt/core.h>

#include "folder_diff.h"

namespace app {

namespace fs = std::filesystem;

// when a file intent changes action, we update the counts for each
// category for faster lookup by the gui
void FolderDiff::UpdateActionCount(FileIntent::Action action, int delta) {
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

// when adding an intent, we need to keep track of whether it produces a potential conflict
// we do this by setting the dirty flag, and incrementing a counter
void FolderDiff::AddIntent(FileIntent &intent) {
    is_conflict_table_dirty = true;
    intents[intent.src] = intent; 
    if (intent.action == FileIntent::Action::RENAME) {
        upcoming_counts[intent.dest]++;
    }
    UpdateActionCount(intent.action, +1);
}

// when changing an intent, we need to keep track of whether it becomes or stops being a conflict
// we do this by setting the dirty flag, and decrementing/incrementing the counter
void FolderDiff::OnIntentUpdate(FileIntent &intent, FileIntent::Action new_action) {
    if (intent.action == new_action) {
        return;
    }

    UpdateActionCount(intent.action, -1);
    UpdateActionCount(new_action, +1);

    is_conflict_table_dirty = true;

    const auto old_action = intent.action;
    const auto rename_action = FileIntent::Action::RENAME;
    intent.action = new_action;

    // auto fill the destination if renaming and no destination defined
    if ((new_action == rename_action) && (intent.dest.size() == 0)) {
        intent.dest = intent.src;
    }

    // update upcoming counts
    if (old_action != rename_action && new_action == rename_action) {
        upcoming_counts[intent.dest]++;
    } else if (old_action == rename_action && new_action != rename_action) {
        upcoming_counts[intent.dest]--;
    } 
}

// same as change of intent's actions
void FolderDiff::OnIntentUpdate(FileIntent &intent, bool new_is_active) {
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

// if the conflict table has been labelled as dirty
// we can run this to bring it up to speed
void FolderDiff::UpdateConflictTable() {
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

}