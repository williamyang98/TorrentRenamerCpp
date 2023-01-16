#include "app_file_state.h"
#include "app_folder_state.h"
#include "file_intents.h"

namespace app
{

AppFileState::AppFileState(AppFolderState& folder, FileIntent&& intent)
: m_folder(folder), m_intent(intent) {
    m_old_dest = m_intent.dest;
}

AppFileState::~AppFileState() = default;

void AppFileState::SetAction(FileIntent::Action new_action) {
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
        m_folder.upcoming_rename_counts[m_intent.dest]++;
    } else if (old_action == rename_action && new_action != rename_action) {
        m_folder.upcoming_rename_counts[m_intent.dest]--;
    } 
}

void AppFileState::SetIsActive(bool new_is_active) {
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
        m_folder.upcoming_rename_counts[m_intent.dest]++;
    } else {
        m_folder.upcoming_rename_counts[m_intent.dest]--;
    } 
}

void AppFileState::OnDestChange() {
    m_folder.is_conflict_table_dirty = true;
    const bool is_rename = (m_intent.action == FileIntent::Action::RENAME);
    if (m_intent.is_active && is_rename) {
        m_folder.upcoming_rename_counts[m_old_dest]--;
        m_folder.upcoming_rename_counts[m_intent.dest]++;
    }
    m_old_dest = m_intent.dest;
}

};