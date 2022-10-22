#include "managed_folder.h"

#include <vector>
#include <filesystem>
#include <string>
#include <fmt/core.h>

namespace app {

namespace fs = std::filesystem;

void ManagedFileIntent::SetAction(FileIntent::Action new_action) {
    if (m_intent.action == new_action) {
        return;
    }

    m_folder.UpdateActionCount(m_intent.action, -1);
    m_folder.UpdateActionCount(new_action, +1);

    m_folder.is_conflict_table_dirty = true;

    const auto old_action = m_intent.action;
    const auto rename_action = FileIntent::Action::RENAME;
    m_intent.action = new_action;

    // auto fill the destination if renaming and no destination defined
    if ((new_action == rename_action) && (m_intent.dest.size() == 0)) {
        m_intent.dest = m_intent.src;
        m_old_dest = m_intent.dest;
    }

    if (!m_intent.is_active) {
        return;
    }

    // update upcoming counts if it is active
    if (old_action != rename_action && new_action == rename_action) {
        m_folder.upcoming_counts[m_intent.dest]++;
    } else if (old_action == rename_action && new_action != rename_action) {
        m_folder.upcoming_counts[m_intent.dest]--;
    } 
}

void ManagedFileIntent::SetIsActive(bool new_is_active) {
    if (m_intent.is_active == new_is_active) {
        return;
    }

    m_intent.is_active = new_is_active;

    if (m_intent.action != FileIntent::Action::RENAME) {
        return;
    }

    m_folder.is_conflict_table_dirty = true;

    // update upcoming counts
    if (new_is_active) {
        m_folder.upcoming_counts[m_intent.dest]++;
    } else {
        m_folder.upcoming_counts[m_intent.dest]--;
    } 
}

void ManagedFileIntent::OnDestChange() {
    m_folder.is_conflict_table_dirty = true;
    const bool is_rename = (m_intent.action == FileIntent::Action::RENAME);
    if (m_intent.is_active && is_rename) {
        m_folder.upcoming_counts[m_old_dest]--;
        m_folder.upcoming_counts[m_intent.dest]++;
    }
    m_old_dest = m_intent.dest;
}

// when adding an intent, we need to keep track of whether it produces a potential conflict
// we do this by setting the dirty flag, and incrementing a counter
void ManagedFolder::AddIntent(FileIntent &intent) {
    is_conflict_table_dirty = true;
    // intent object is now invalid since managed file intent transfers ownership
    auto managed_intent = ManagedFileIntent(*this, intent);
    
    const bool is_rename = (managed_intent.GetAction() == FileIntent::Action::RENAME);
    if (is_rename && managed_intent.GetIsActive()) {
        upcoming_counts[managed_intent.GetDest()]++;
    }
    UpdateActionCount(managed_intent.GetAction(), +1);
    // insert intent at the end to avoid invalid references
    intents.insert({managed_intent.GetSrc(), std::move(managed_intent)});
}

ConflictTable& ManagedFolder::GetConflicts() {
    UpdateConflictTable();
    return conflicts;
}

// if the conflict table has been labelled as dirty
// we can run this to bring it up to speed
void ManagedFolder::UpdateConflictTable() {
    if (!is_conflict_table_dirty) {
        return;
    }

    // clear the conflicts table
    conflicts.clear();

    for (auto &[key, intent]: intents) {
        const bool is_rename = (intent.GetAction() == FileIntent::Action::RENAME);

        // source conflicts with upcoming change, and it isn't a rename action
        if (!is_rename && (upcoming_counts[intent.GetSrc()] > 0)) {
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
            (upcoming_counts[intent.GetDest()] > 1) ||
            (intents.find(intent.GetDest()) != intents.end());
        
        intent.SetIsConflict(is_dst_conflict);
        if (is_dst_conflict) {
            conflicts[intent.GetDest()].push_back(key);
            continue;
        }
    }

    is_conflict_table_dirty = false;
}

// when a file intent changes action, we update the counts for each
// category for faster lookup by the gui
void ManagedFolder::UpdateActionCount(FileIntent::Action action, int delta) {
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
