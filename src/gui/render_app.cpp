#include "render_app.h"
#include "app/app_folder.h"

#include <stdio.h>
#include <string>
#include <mutex>
#include <array>
#include <filesystem>
#include <optional>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <fmt/format.h>

#include "app/app_folder_bookmarks.h"
#include "font_awesome_definitions.h"
#include "tvdb_api/tvdb_models.h"

#include "os_dep.h"

namespace app::gui 
{

constexpr int MAX_BUFFER_SIZE = 256;
namespace fs = std::filesystem;

// render components
static void RenderSeriesList(App& main_app);
static void RenderSeriesSelectModal(App& main_app, AppFolder& folder);
static void RenderEpisodes(App& main_app);
static void RenderEpisodesGenericList(AppFolder& folder, const char* table_id, FileIntent::Action action, ImGuiTextFilter& search_filter);
static void RenderFilesComplete(AppFolder& folder);
static void RenderFilesIgnore(AppFolder& folder);
static void RenderFilesRename(AppFolder& folder);
static void RenderFilesDelete(AppFolder& folder);
static void RenderFilesConflict(AppFolder& folder);
static void RenderFilesWhitelist(AppFolder& folder);
static void RenderFileContextMenu(AppFolder& folder, AppFileState& intent, const char* label);
static void RenderSeriesInfo(App& main_app);
static void RenderEpisodeInfo(App& main_app);
static void RenderErrors(App& main_app);
static void RenderAppWarnings(App& main_app);
static void RenderAppErrors(App& main_app);

// our global filters
struct {
    ImGuiTextFilter completes;
    ImGuiTextFilter ignores;
    ImGuiTextFilter renames;
    ImGuiTextFilter deletes;
    ImGuiTextFilter conflicts;
    ImGuiTextFilter whitelists;

    void ClearAll() {
        completes.Clear();
        ignores.Clear();
        renames.Clear();
        deletes.Clear();
        conflicts.Clear();
        whitelists.Clear();
    }
} CategoryFilters;

struct FileActionStringPair {
    FileIntent::Action action;
    const char* str;
    const char* shortcut;
};

std::array<FileActionStringPair, 5> FileActionToString = {{
    { FileIntent::Action::DELETE, "Delete", "Del" },
    { FileIntent::Action::IGNORE, "Ignore", "Alt+i" },
    { FileIntent::Action::RENAME, "Rename", "Alt+r" },
    { FileIntent::Action::WHITELIST, "Whitelist", "Alt+w" },
    { FileIntent::Action::COMPLETE, "Complete", "Alt+c" },
}};

// our global colors
const ImU32 CONFLICT_BG_COLOR = 0x6F0000FF;

struct FolderStatusCharacter {
    ImColor color;
    const char* chr;
};

static FolderStatusCharacter GetFolderStatusCharacter(AppFolder::Status status) {
    switch (status) {
    case AppFolder::Status::UNKNOWN:            return { ImColor(  0,  0,255), ICON_FA_QUESTION_CIRCLE};
    case AppFolder::Status::PENDING_DELETES:    return { ImColor(255,  0,  0), ICON_FA_RECYCLE };
    case AppFolder::Status::CONFLICTS:          return { ImColor(255,215,  0), ICON_FA_EXCLAMATION_TRIANGLE };
    case AppFolder::Status::PENDING_RENAME:     return { ImColor(  0,255,255), ICON_FA_INFO_CIRCLE };
    case AppFolder::Status::COMPLETED:          return { ImColor(  0,255,  0), ICON_FA_CHECK };
    case AppFolder::Status::EMPTY:              return { ImColor(  0,  0,  0), ICON_FA_CHEVRON_CIRCLE_RIGHT };
    default:                                    return { ImColor(  0,  0,  0), ICON_FA_CHEVRON_CIRCLE_RIGHT };
    }
}

void RenderApp(App& main_app) {
    // render out of order to get last item as default focus
    RenderAppWarnings(main_app);
    RenderSeriesList(main_app);

    ImGui::Begin("Episodes panel");
    RenderEpisodes(main_app);
    ImGui::End();

    ImGui::Begin("Series info");
    RenderSeriesInfo(main_app);
    ImGui::End();

    ImGui::Begin("Episode info");
    RenderEpisodeInfo(main_app);
    ImGui::End();

    ImGui::Begin("Errors");
    RenderErrors(main_app);
    ImGui::End();

    RenderAppErrors(main_app);
}

void RenderSeriesList(App& main_app) {
    auto& folders = main_app.m_folders;
    static char LABEL_BUFFER[MAX_BUFFER_SIZE+1] = {0};

    snprintf(
        LABEL_BUFFER, MAX_BUFFER_SIZE,
        "Series (%zu)###Series", folders.size());
    
    ImGuiWindowFlags win_flags = ImGuiWindowFlags_MenuBar;
    ImGui::Begin(LABEL_BUFFER, NULL, win_flags);

    const int busy_count = main_app.get_folder_busy_count();
    ImGui::BeginDisabled(busy_count > 0);
    
    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Select folder")) {
            auto opt = os_dep::open_folder_dialog();
            if (opt) {
                main_app.m_root = fs::path(std::move(opt.value()));
                main_app.refresh_folders();
            }
        }
        ImGui::EndMenuBar();
    }

    if (ImGui::Button("Refresh project structure")) {
        main_app.refresh_folders();
    }

    if (ImGui::Button("Scan contents of all folders")) {
        for (auto& folder: folders) {
            main_app.queue_async_call([&folder](int pid) {
                folder->update_state_from_cache();
            });
        }
    }
    ImGui::EndDisabled();

    ImGui::Text("Total busy folders (%d/%zu)", busy_count, folders.size());

    ImGui::Separator();
    static ImGuiTextFilter search_filter;
    search_filter.Draw();

    // Render our status checkboxes for filtering
    static uint32_t search_status_flags = 0xFF;
    if (ImGui::BeginTable("##status filter", 2)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::CheckboxFlags("Completed", &search_status_flags, AppFolder::Status::COMPLETED);
        ImGui::TableSetColumnIndex(1);
        ImGui::CheckboxFlags("Pending", &search_status_flags, AppFolder::Status::PENDING_RENAME);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::CheckboxFlags("Deletes",   &search_status_flags, AppFolder::Status::PENDING_DELETES);
        ImGui::TableSetColumnIndex(1);
        ImGui::CheckboxFlags("Conflicts", &search_status_flags, AppFolder::Status::CONFLICTS);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::CheckboxFlags("Unknown", &search_status_flags, AppFolder::Status::UNKNOWN);
        ImGui::TableSetColumnIndex(1);
        ImGui::CheckboxFlags("Empty", &search_status_flags, AppFolder::Status::EMPTY);
        ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::BeginListBox("##series_list_box", ImVec2(-1,-1))) {

        int folder_id = 0;
        for (auto& folder: folders) {
            auto folder_name = folder->GetPath().filename().string();

            const auto status_info = GetFolderStatusCharacter(folder->m_status);

            if ((folder->m_status & search_status_flags) == 0) {
                continue;
            }

            if (!search_filter.PassFilter(folder_name.c_str())) {
                continue;
            }

            auto is_selected = main_app.m_current_folder == folder;
            ImGui::PushID(folder_id++);
            
            // status
            ImGui::PushStyleColor(ImGuiCol_Text, status_info.color.Value);
            bool selected_pressed = ImGui::Selectable(status_info.chr, is_selected);
            ImGui::PopStyleColor();
            // open context menu for additional folder things
            snprintf(LABEL_BUFFER, MAX_BUFFER_SIZE, "###series_folder_context_menu_%d", folder_id);
            if (ImGui::BeginPopupContextItem(LABEL_BUFFER)) {
                if (ImGui::MenuItem("Open Folder")) {
                    os_dep::open_folder(folder->GetPath().string());
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            // folder name
            ImGui::Text("%s", folder_name.c_str());
            if (selected_pressed) {
                main_app.m_current_folder = folder;
                main_app.queue_async_call([&folder](int pid) {
                    folder->update_state_from_cache();
                    folder->load_bookmarks_from_file();
                });
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    ImGui::End();
}

void RenderFileContextMenu(AppFolder& folder, AppFileState& intent, const char* label) {
    if (ImGui::IsItemHovered()) {
        // Shortcuts
        if (intent.GetAction() != FileIntent::Action::DELETE) {
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                intent.SetAction(FileIntent::Action::DELETE);
                intent.SetIsActive(false);
            }
        }
        if (intent.GetAction() != FileIntent::Action::RENAME) {
            if (ImGui::IsKeyPressed(ImGuiKey_R, false) && ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
                intent.SetAction(FileIntent::Action::RENAME);
                intent.SetIsActive(false);
            }
        }
        if (intent.GetAction() != FileIntent::Action::IGNORE) {
            if (ImGui::IsKeyPressed(ImGuiKey_I, false) && ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
                intent.SetAction(FileIntent::Action::IGNORE);
                intent.SetIsActive(false);
            }
        }
        if (intent.GetAction() != FileIntent::Action::WHITELIST) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false) && ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
                intent.SetAction(FileIntent::Action::WHITELIST);
                intent.SetIsActive(false);
            }
        }
    }

    const auto& style = ImGui::GetStyle();
    
    if (ImGui::BeginPopupContextItem()) {
        const auto& filename = intent.GetSrc();

        if (ImGui::Selectable("Open folder")) {
            folder.open_folder(filename);
        }
        if (ImGui::Selectable("Open file")) {
            folder.open_file(filename);
        }

        ImGui::Separator();

        for (auto& p: FileActionToString) {
            if (intent.GetAction() == p.action) {
                continue;
            }

            const bool is_pressed = ImGui::Selectable(p.str, false, 0);
            ImGui::SameLine();
            static const int WINDOW_WIDTH = ImGui::CalcTextSize("Whitelist         Alt+w").x;
            ImGui::SetCursorPosX(WINDOW_WIDTH - ImGui::CalcTextSize(p.shortcut).x);
            ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
            ImGui::Text("%s", p.shortcut);
            ImGui::PopStyleColor();
            if (is_pressed) {
                intent.SetAction(p.action);
                intent.SetIsActive(false);
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

void RenderEpisodes(App& main_app) {
    if (main_app.m_current_folder == nullptr) {
        ImGui::Text("Please select a series");
        return;
    }
    
    // if the selected folder changed, we reset the filters
    static AppFolder* prev_folder = nullptr;
    if (prev_folder != main_app.m_current_folder.get()) {
        CategoryFilters.ClearAll();
    }
    prev_folder = main_app.m_current_folder.get();

    auto& folder = *main_app.m_current_folder;
    bool is_busy = folder.m_is_busy;

    ImGui::BeginDisabled(is_busy);

    if (ImGui::Button("Scan for changes")) {
        main_app.queue_async_call([&folder](int pid) {
            folder.update_state_from_cache();
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh from cache")) {
        main_app.queue_async_call([&folder](int pid) {
            folder.load_cache_from_file();
            folder.update_state_from_cache();
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Download and refresh from tvdb")) {
        main_app.queue_async_call([&folder, &main_app](int pid) {
            folder.load_cache_from_tvdb(folder.m_cache.series.id, main_app.m_token.c_str());
            folder.update_state_from_cache();
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Execute changes")) {
        main_app.queue_async_call([&folder, &main_app](int pid) {
            folder.execute_actions();
            folder.update_state_from_cache();
        });
    }

    ImGui::SameLine();
    RenderSeriesSelectModal(main_app, folder);

    ImGui::EndDisabled();

    ImGui::Separator();

    // render the state tree
    std::scoped_lock state_lock(folder.m_state_mutex);
    std::scoped_lock bookmarks_lock(folder.m_bookmarks_mutex);

    auto& state = folder.m_state;
    auto& counts = state->GetActionCount();

    bool show_tab_bar = ImGui::BeginTabBar("##file intent tab group");

    auto tab_name = fmt::format("Completed {:d}###completed tab", counts.completes);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        ImGui::BeginChild("##complete tab");
        RenderFilesComplete(folder);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Pending {:d}###pending tab", counts.renames);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        ImGui::BeginChild("##pending tab");
        RenderFilesRename(folder);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Conflicts {:d}###conflict tab", state->GetConflicts().size());
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        ImGui::BeginChild("##conflict tab");
        RenderFilesConflict(folder);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Deletes {:d}###delete tab", counts.deletes);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        ImGui::BeginChild("##delete tab");
        RenderFilesDelete(folder);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Ignores {:d}###ignore tab", counts.ignores);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        ImGui::BeginChild("##ignore tab");
        RenderFilesIgnore(folder);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Whitelists {:d}###whitelist tab", counts.whitelists);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        ImGui::BeginChild("##whitelist tab");
        RenderFilesWhitelist(folder);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (show_tab_bar) {
        ImGui::EndTabBar();
    }
    
    if (folder.m_bookmarks.m_is_dirty) {
        folder.m_bookmarks.m_is_dirty = false;
        main_app.queue_async_call([&folder, &main_app](int pid) {
            folder.save_bookmarks_to_file();
        });
    }
}

void RenderBookmarks(AppFolderBookmarks& bookmarks, const std::string& filename) {
    auto& flags = bookmarks.GetFlags(filename);
    bool is_changed = false;

    const auto& style = ImGui::GetStyle();
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x/2, style.ItemSpacing.y));

    if (flags.is_starred) ImGui::PushStyleColor(ImGuiCol_Text, ImColor(245,206,66).Value);
    else                  ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    ImGui::Text(ICON_FA_STAR);
    if (ImGui::IsItemClicked()) {
        flags.is_starred = !flags.is_starred;
        is_changed = true;
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();

    if (flags.is_unread) ImGui::PushStyleColor(ImGuiCol_Text, ImColor(255,0,0).Value);
    else                 ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    ImGui::Text(ICON_FA_QUESTION);
    if (ImGui::IsItemClicked()) {
        flags.is_unread = !flags.is_unread;
        is_changed = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();

    if (flags.is_read) ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0,255,0).Value);
    else               ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    ImGui::Text(ICON_FA_CHECK);
    if (ImGui::IsItemClicked()) {
        flags.is_read = !flags.is_read;
        is_changed = true;
    }
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
    
    if (is_changed) {
        bookmarks.m_is_dirty = true;
    }
}

void RenderEpisodesGenericList(AppFolder& folder, const char* table_id, FileIntent::Action action, ImGuiTextFilter& search_filter) {
    search_filter.Draw();

    auto& state = folder.m_state;
    
    ImGui::BeginChild("##intent_table");

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable(table_id, 1, flags)) {
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int i = 0;
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != action) continue; 
            const char* name = intent.GetSrc().c_str(); 
            if (!search_filter.PassFilter(name)) {
                continue;
            }

            ImGui::TableNextRow();

            const bool is_conflict = intent.GetIsConflict();
            if (is_conflict) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, CONFLICT_BG_COLOR);
            }

            ImGui::PushID(i++);
            ImGui::TableSetColumnIndex(0);
            const char* popup_key = "##intent action popup";
            const bool is_selected = folder.selected_episode.has_value() && (folder.selected_episode == intent.GetIntent().descriptor);

            RenderBookmarks(folder.m_bookmarks, intent.GetSrc());
            ImGui::SameLine();
            if (ImGui::Selectable(name, is_selected)) {
                if (is_selected) {
                    folder.selected_episode = std::nullopt;
                } else {
                    folder.selected_episode = intent.GetIntent().descriptor;
                }
            }
            RenderFileContextMenu(folder, intent, popup_key);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void RenderFilesComplete(AppFolder& folder) {
    RenderEpisodesGenericList(folder, "##completed table", FileIntent::Action::COMPLETE, CategoryFilters.completes);
}

void RenderFilesIgnore(AppFolder& folder) {
    RenderEpisodesGenericList(folder, "##ignore table", FileIntent::Action::IGNORE, CategoryFilters.ignores);
}

void RenderFilesWhitelist(AppFolder& folder) {
    RenderEpisodesGenericList(folder, "##whitelist table", FileIntent::Action::WHITELIST, CategoryFilters.whitelists);
}

void RenderFilesRename(AppFolder& folder) {
    auto& state = folder.m_state;
    if (ImGui::Button("Select all")) {
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != FileIntent::Action::RENAME) continue; 
            intent.SetIsActive(true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) {
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != FileIntent::Action::RENAME) continue; 
            intent.SetIsActive(false);
        }
    }
    ImGui::Separator();

    ImGuiTextFilter& search_filter = CategoryFilters.renames;
    search_filter.Draw();
    ImGui::BeginChild("##intent_table");

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Pending Table", 3, flags)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int row_id  = 0;
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != FileIntent::Action::RENAME) continue; 

            const char* src_name = intent.GetSrc().c_str();
            const char* dest_name = intent.GetDest().c_str();
            if (!search_filter.PassFilter(src_name) &&
                !search_filter.PassFilter(dest_name))
            {
                continue;
            }

            ImGui::PushID(row_id++);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            const bool is_conflict = intent.GetIsConflict();
            if (is_conflict) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, CONFLICT_BG_COLOR);
            }

            bool is_active_copy = intent.GetIsActive();
            if (ImGui::Checkbox("##intent_checkbox", &is_active_copy)) {
                intent.SetIsActive(is_active_copy);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", src_name);
            ImGui::TableSetColumnIndex(2);

            ImGui::PushItemWidth(-1.0f);
            if (ImGui::InputText("###dest path", &intent.GetDest())) {
                intent.OnDestChange();
            }
            ImGui::PopItemWidth();

            const char* popup_id = "##intent action popup";
            ImGui::SameLine();
            const bool is_selected = folder.selected_episode.has_value() && (folder.selected_episode == intent.GetIntent().descriptor);
            if (ImGui::Selectable("###row popup button", is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                if (is_selected) {
                    folder.selected_episode = std::nullopt;
                } else {
                    folder.selected_episode = intent.GetIntent().descriptor;
                }
            }
            RenderFileContextMenu(folder, intent, popup_id);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void RenderFilesDelete(AppFolder& folder) {
    auto& state = folder.m_state;

    if (ImGui::Button("Select all")) {
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != FileIntent::Action::DELETE) continue; 
            intent.SetIsActive(true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) {
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != FileIntent::Action::DELETE) continue; 
            intent.SetIsActive(false);
        }
    }
    ImGui::Separator();

    ImGuiTextFilter& search_filter = CategoryFilters.deletes;
    search_filter.Draw();

    ImGui::BeginChild("##intent_table");

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("##delete table", 2, flags)) {
        ImGui::TableSetupColumn("##is active checkbox", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int i = 0;
        for (auto& [key, intent]: state->GetIntents()) {
            if (intent.GetAction() != FileIntent::Action::DELETE) continue; 

            const char* src_name = intent.GetSrc().c_str();
            const char* dest_name = intent.GetDest().c_str();
            if (!search_filter.PassFilter(src_name) &&
                !search_filter.PassFilter(dest_name))
            {
                continue;
            }

            ImGui::TableNextRow();

            const bool is_conflict = intent.GetIsConflict();
            if (is_conflict) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, CONFLICT_BG_COLOR);
            }

            ImGui::PushID(i++);
            ImGui::TableSetColumnIndex(0);

            bool is_active_copy = intent.GetIsActive();
            if (ImGui::Checkbox("##active checkbox", &is_active_copy)) {
                intent.SetIsActive(is_active_copy);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", intent.GetSrc().c_str());
            ImGui::SameLine();

            const char* popup_id = "##intent action popup";
            const bool is_selected = folder.selected_episode.has_value() && (folder.selected_episode == intent.GetIntent().descriptor);
            if (ImGui::Selectable("##action popup row", false, ImGuiSelectableFlags_SpanAllColumns)) {
                if (is_selected) {
                    folder.selected_episode = std::nullopt;
                } else {
                    folder.selected_episode = intent.GetIntent().descriptor;
                }
            }

            RenderFileContextMenu(folder, intent, popup_id);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void RenderFilesConflict(AppFolder& folder) {
    auto& state = folder.m_state;
    auto& conflicts = state->GetConflicts();

    ImGuiTextFilter& search_filter = CategoryFilters.conflicts;
    search_filter.Draw();

    ImGui::BeginChild("Conflict tab");

    const ImGuiTableFlags flags = 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | 
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_SizingStretchProp;
    
    for (auto& [dest, targets]: conflicts) {
        auto tree_label = fmt::format("{} ({:d})", dest.c_str(), targets.size());
        if (ImGui::CollapsingHeader(tree_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##conflict_table", 3, flags)) {

                ImGui::TableSetupColumn("##active_checkbox_column", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                auto& intents = state->GetIntents();
                ImGui::PushID(dest.c_str());
                for (auto& key: targets) {
                    auto res = intents.find(key);
                    if (res == intents.end()) {
                        continue;
                    }

                    auto& intent = res->second;
                    if (!search_filter.PassFilter(intent.GetSrc().c_str())) {
                        continue;
                    }

                    const bool is_rename = intent.GetAction() == FileIntent::Action::RENAME;
                    
                    ImGui::PushID(key.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    if (is_rename) {
                        bool is_active_copy = intent.GetIsActive();
                        if (ImGui::Checkbox("##active_check", &is_active_copy)) {
                            intent.SetIsActive(is_active_copy);
                        }
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped("%s", intent.GetSrc().c_str());

                    ImGui::TableSetColumnIndex(2);
                    if (is_rename) {
                        ImGui::PushItemWidth(-1.0f);
                        if (ImGui::InputText("###dest path", &intent.GetDest())) {
                            intent.OnDestChange();
                        }
                        ImGui::PopItemWidth();
                    } else {
                        //ImGui::TextWrapped("%s", intent.GetDest().c_str());
                    }

                    auto popup_label = fmt::format("##action popup_{}", key.c_str());
                    ImGui::SameLine();
                    if (ImGui::Selectable("##action popup select", false, ImGuiSelectableFlags_SpanAllColumns)) {
                        /* ImGui::OpenPopup(popup_label.c_str()); */
                    }

                    RenderFileContextMenu(folder, intent, popup_label.c_str());
                    ImGui::PopID();
                }
                ImGui::PopID();

                ImGui::EndTable();
            }
        }
        ImGui::Spacing();
    }

    ImGui::EndChild();
}

void RenderSeriesInfo(App& main_app) {
    if (main_app.m_current_folder == nullptr) {
        return;
    }

    auto& folder = *main_app.m_current_folder;
    std::scoped_lock cache_lock(folder.m_cache_mutex);
    const auto& cache = folder.m_cache;
    const auto& is_cached = folder.m_is_info_cached;
    if (!is_cached) {
        ImGui::TextWrapped("Cache is missing");
        return;
    }

    const auto& series = cache.series;
    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("Series", 2, flags)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("ID"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%d", series.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Name"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", series.name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Status"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", series.status.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Air Date"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", series.air_date.c_str());

        if (series.overview) {
            auto& label = series.overview.value();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Overview"); 
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", label.c_str());
        }

        ImGui::EndTable();
    }

}

void RenderEpisodeInfo(App& main_app) {
    if (main_app.m_current_folder == nullptr) {
        return;
    }

    auto& folder = *main_app.m_current_folder;
    std::scoped_lock cache_lock(folder.m_cache_mutex);
    const auto& cache = folder.m_cache;
    const auto& is_cached = folder.m_is_info_cached;
    if (!is_cached) {
        ImGui::TextWrapped("Cache is missing");
        return;
    }

    auto& episodes = cache.episodes;
    if (!folder.selected_episode.has_value()) {
        ImGui::TextWrapped("No episode selected");
        return;
    }
    auto res = episodes.find(folder.selected_episode.value());
    if (res == episodes.end()) {
        ImGui::TextWrapped("No episode selected");    
        return;
    }
    const auto& episode = res->second;

    const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

    if (ImGui::BeginTable("Episodes", 2, flags)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("ID"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%d", episode.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Index"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("S%02dE%02d", episode.season, episode.episode);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Name"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%*s", int(episode.name.size()), episode.name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Air Date"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%*s", int(episode.air_date.size()), episode.air_date.c_str());

        if (episode.overview) {
            auto& label = episode.overview.value();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Overview"); 
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", label.c_str());
        }

        ImGui::EndTable();
    }

}

void RenderErrors(App& main_app) {
    if (main_app.m_current_folder == nullptr) {
        return;
    }

    auto& folder = *main_app.m_current_folder;
    std::scoped_lock errors_lock(folder.m_errors_mutex);
    auto& errors = folder.m_errors;

    ImGui::Text("Error List");
    if (ImGui::BeginListBox("##Error List", ImVec2(-1,-1))) {
        
        auto it = errors.begin();
        auto end = errors.end();

        int gid = 0;
        while (it != end) {
            auto& error = *it;

            ImGui::PushID(gid++);

            bool is_pressed = ImGui::Button("X");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", error.c_str());

            if (is_pressed) {
                it = errors.erase(it);
            } else {
                ++it;
            }

            ImGui::PopID();
        }

        ImGui::EndListBox();
    }
}

void RenderSeriesSelectModal(App& main_app, AppFolder& folder) {
    static const char* modal_title = "Select a series###series selection modal";

    if (ImGui::Button("Select Series")) {
        ImGui::OpenPopup(modal_title);
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    static char search_string[256] = {0};
    const char* buf = (const char*)(&search_string[0]);

    bool is_modal_open = true;

    if (ImGui::BeginPopupModal(modal_title, &is_modal_open, 0)) {
        ImGui::InputText("##search_text", search_string, IM_ARRAYSIZE(search_string));

        ImGui::SameLine();
        if (ImGui::Button("Search")) {
            main_app.queue_async_call([&folder, buf, &main_app](int pid) {
                folder.load_search_series_from_tvdb(buf, main_app.m_token.c_str());
            });
        }
        ImGui::Separator();

        ImGuiTableFlags flags = 
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | 
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
            ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;

        if (ImGui::BeginTable("##Search Results", 5, flags)) {
            ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Date",     ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ID",       ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Status",   ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Action",   ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
        
            std::scoped_lock search_lock(folder.m_search_mutex);
            int sid = 0;
            for (auto& r: folder.m_search_result) {
                ImGui::TableNextRow();
                ImGui::PushID(sid++);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", r.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", r.air_date.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", r.id);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", r.status.c_str());
                ImGui::TableSetColumnIndex(4);
                if (ImGui::Button("Select")) {
                    uint32_t id = r.id;
                    main_app.queue_async_call([id, &folder, &main_app](int pid) {
                        folder.load_cache_from_tvdb(id, main_app.m_token.c_str());
                        folder.update_state_from_cache();
                    });
                    ImGui::CloseCurrentPopup(); 
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
        ImGui::EndPopup();
    }
}

void RenderAppWarnings(App& main_app) {
    auto lock = std::scoped_lock(main_app.m_app_warnings_mutex); 
    auto& warnings = main_app.m_app_warnings;
    static char window_name[MAX_BUFFER_SIZE+1] = {0};
    snprintf(
            window_name, MAX_BUFFER_SIZE, 
            "Warnings (%zu)###application warnings", warnings.size()); 

    ImGui::Begin(window_name);
    if (ImGui::BeginListBox("##Warning List", ImVec2(-1,-1))) {
        auto it = warnings.begin();
        auto end = warnings.end();

        int gid = 0;
        while (it != end) {
            auto& warning = *it;

            ImGui::PushID(gid++);

            bool is_pressed = ImGui::Button("X");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", warning.c_str());

            if (is_pressed) {
                it = warnings.erase(it);
            } else {
                ++it;
            }

            ImGui::PopID();
        }

        ImGui::EndListBox();
    }
    ImGui::End();
}

void RenderAppErrors(App& main_app) {
    static const char* modal_title = "Application error###app error modal";

    std::scoped_lock error_lock(main_app.m_app_errors_mutex); 
    auto& errors = main_app.m_app_errors;
    if (errors.size() == 0) {
        return;
    }

    ImGui::OpenPopup(modal_title);
    
    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(modal_title, NULL, 0)) {
        ImGui::Text("The application has encounted an error");
        ImGui::Text("Please restart the application");
        ImGui::Separator();
        if (ImGui::BeginListBox("##app error list", ImVec2(-1,-1))) {
            for (const auto& e: errors) {
                ImGui::TextWrapped("%s", e.c_str());
            }

            ImGui::EndListBox();
        }
        ImGui::EndPopup();
    }
}

};
