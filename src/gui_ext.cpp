#include <iostream>
#include <string>
#include <mutex>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <fmt/format.h>

#include "gui_ext.h"
#include "font_awesome_definitions.h"

#include <array>

namespace app::gui 
{

static void RenderSeriesList(App &main_app);

static void RenderSeriesFolder(App &main_app, SeriesFolder &folder);
static void RenderEpisodes(App &main_app, SeriesFolder &folder);
static void RenderInfoPanel(App &main_app, SeriesFolder &folder);

static void RenderFilesComplete(FolderDiff &state);
static void RenderFilesIgnore(FolderDiff &state);
static void RenderFilesRename(FolderDiff &state);
static void RenderFilesDelete(FolderDiff &state);
static void RenderFilesConflict(FolderDiff &state);
static void RenderFilesWhitelist(FolderDiff &state);

static void RenderCacheInfo(App &main_app, SeriesFolder &folder);
static void RenderErrors(App &main_app, SeriesFolder &folder);

static void RenderSeriesSelectModal(App &main_app, SeriesFolder &folder);

static void RenderAppErrors(App &main_app);


void RenderApp(App &main_app) {
    ImGui::Begin("Series");
    RenderSeriesList(main_app);
    ImGui::End();

    
    ImGui::Begin("Series folder");
    if (main_app.m_current_folder != nullptr) {
        RenderSeriesFolder(main_app, *main_app.m_current_folder);
    } else {
        ImGui::Text("Select a series");
    }
    ImGui::End();

    RenderAppErrors(main_app);
}

struct FolderStatusCharacter {
    const char * chr;
    ImColor color;
};

static FolderStatusCharacter GetFolderStatusCharacter(SeriesFolder::Status status) {
    switch (status) {
    case SeriesFolder::Status::UNKNOWN: 
        return { ICON_FA_QUESTION_CIRCLE, ImColor(0,0,255) };
    case SeriesFolder::Status::PENDING_DELETES: 
        return { ICON_FA_RECYCLE, ImColor(255,0,0) };
    case SeriesFolder::Status::CONFLICTS: 
        return { ICON_FA_EXCLAMATION_TRIANGLE, ImColor(255,215,0) };
    case SeriesFolder::Status::PENDING_RENAME: 
        return { ICON_FA_INFO_CIRCLE, ImColor(0,255,255) };
    case SeriesFolder::Status::COMPLETED: 
        return { ICON_FA_CHECK, ImColor(0,255,0) };
    case SeriesFolder::Status::EMPTY:
    default:
        return { ICON_FA_CHEVRON_CIRCLE_RIGHT, ImColor(0,0,0) };
    }
}

void RenderSeriesList(App &main_app) {
    const int busy_count = main_app.get_folder_busy_count();
    ImGui::BeginDisabled(busy_count > 0);
    if (ImGui::Button("Refresh project structure")) {
        main_app.refresh_folders();
    }

    if (ImGui::Button("Scan contents of all folders")) {
        for (auto &folder: main_app.m_folders) {
            main_app.queue_async_call([&folder](int pid) {
                folder.update_state_from_cache();
            });
        }
    }
    ImGui::EndDisabled();

    ImGui::Text("Total busy folders (%d/%d)", busy_count, main_app.m_folders.size());

    ImGui::Separator();
    static ImGuiTextFilter search_filter;
    search_filter.Draw();

    // Render our status checkboxes for filtering
    static uint32_t search_status_flags = 0xFF;
    if (ImGui::BeginTable("##status filter", 2)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::CheckboxFlags("Completed", &search_status_flags, SeriesFolder::Status::COMPLETED);
        ImGui::TableSetColumnIndex(1);
        ImGui::CheckboxFlags("Pending",   &search_status_flags, SeriesFolder::Status::PENDING_RENAME);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::CheckboxFlags("Deletes",   &search_status_flags, SeriesFolder::Status::PENDING_DELETES);
        ImGui::TableSetColumnIndex(1);
        ImGui::CheckboxFlags("Conflicts", &search_status_flags, SeriesFolder::Status::CONFLICTS);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::CheckboxFlags("Unknown", &search_status_flags, SeriesFolder::Status::UNKNOWN);
        ImGui::TableSetColumnIndex(1);
        ImGui::CheckboxFlags("Empty", &search_status_flags, SeriesFolder::Status::EMPTY);
        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::BeginListBox("##series_list_box", ImVec2(-1,-1))) {

        int folder_id = 0;
        for (auto &folder: main_app.m_folders) {
            auto folder_name = folder.m_path.filename().string();

            const auto status_info = GetFolderStatusCharacter(folder.m_status);

            auto label = fmt::format("{} {}", status_info.chr, folder_name.c_str());

            if ((folder.m_status & search_status_flags) == 0) {
                continue;
            }

            if (!search_filter.PassFilter(folder_name.c_str())) {
                continue;
            }

            auto is_selected = main_app.m_current_folder == &folder;
    
            ImGui::PushID(folder_id++);
            ImGui::PushStyleColor(ImGuiCol_Text, status_info.color.Value);
            bool selected_pressed = ImGui::Selectable(status_info.chr, is_selected);
            ImGui::PopStyleColor();
            ImGui::PopID();

            ImGui::SameLine();
            ImGui::Text(folder_name.c_str());

            if (selected_pressed) {
                main_app.m_current_folder = &folder;
                main_app.queue_async_call([&folder](int pid) {
                    folder.update_state_from_cache();
                });
            }
        }
        ImGui::EndListBox();
    }
}

void RenderSeriesFolder(App &main_app, SeriesFolder &folder) {
    ImGuiWindowFlags window_flags = 0;

    float alpha = 0.7f;

    ImGui::BeginChild("Episodes panel", ImVec2(ImGui::GetContentRegionAvail().x*alpha, 0), true, window_flags);
        RenderEpisodes(main_app, folder);
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("Info panel", ImVec2(0, 0), true, window_flags);
    RenderInfoPanel(main_app, folder);
    ImGui::EndChild();

}

struct FileActionStringPair {
    FileIntent::Action action;
    const char *str;
};

std::array<FileActionStringPair, 5> FileActionToString = {{
    { FileIntent::Action::DELETE, "Delete" },
    { FileIntent::Action::IGNORE, "Ignore" },
    { FileIntent::Action::RENAME, "Rename" },
    { FileIntent::Action::COMPLETE, "Complete" },
    { FileIntent::Action::WHITELIST, "Whitelist" },
}};

static void RenderFileIntentChange(FolderDiff &state, FileIntent &intent, const char *label) {
    if (ImGui::BeginPopup(label)) {
        for (auto &p: FileActionToString) {
            if ((intent.action != p.action) 
                && ImGui::Selectable(p.str, false, 0)) 
            {
                state.OnIntentUpdate(intent, p.action);
                state.OnIntentUpdate(intent, false);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

void RenderEpisodes(App &main_app, SeriesFolder &folder) {
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

    // render the state tree
    std::scoped_lock state_lock(folder.m_state_mutex);
    auto &state = folder.m_state;
    auto &counts = state.action_counts;
    state.UpdateConflictTable();

    bool show_tab_bar = ImGui::BeginTabBar("##file intent tab group");

    auto tab_name = fmt::format("Completed {:d}###completed tab", counts.completes);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        RenderFilesComplete(state);
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Pending {:d}###pending tab", counts.renames);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        RenderFilesRename(state);
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Conflicts {:d}###conflict tab", state.conflicts.size());
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        RenderFilesConflict(state);
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Deletes {:d}###delete tab", counts.deletes);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        RenderFilesDelete(state);
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Ignores {:d}###ignore tab", counts.ignores);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        RenderFilesIgnore(state);
        ImGui::EndTabItem();
    }

    tab_name = fmt::format("Whitelists {:d}###whitelsit tab", counts.whitelists);
    if (ImGui::BeginTabItem(tab_name.c_str())) {
        RenderFilesWhitelist(state);
        ImGui::EndTabItem();
    }

    if (show_tab_bar) {
        ImGui::EndTabBar();
    }

}

static void RenderEpisodesGenericList(FolderDiff &state, const char *table_id, FileIntent::Action action) {
    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable(table_id, 1, flags)) {
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int i = 0;
        for (auto &[key, intent]: state.intents) {
            if (intent.action != action) continue; 

            ImGui::TableNextRow();
            ImGui::PushID(i++);
            ImGui::TableSetColumnIndex(0);
            const char *popup_key = "##intent action popup";
            if (ImGui::Selectable(intent.src.c_str())) {
                ImGui::OpenPopup(popup_key);
            }
            RenderFileIntentChange(state, intent, popup_key);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void RenderFilesComplete(FolderDiff &state) {
    RenderEpisodesGenericList(state, "##completed table", FileIntent::Action::COMPLETE);
}

void RenderFilesIgnore(FolderDiff &state) {
    RenderEpisodesGenericList(state, "##ignore table", FileIntent::Action::IGNORE);
}

void RenderFilesWhitelist(FolderDiff &state) {
    RenderEpisodesGenericList(state, "##whitelist table", FileIntent::Action::WHITELIST);
}

void RenderFilesRename(FolderDiff &state) {
    if (ImGui::Button("Select all")) {
        for (auto &[key, intent]: state.intents) {
            if (intent.action != FileIntent::Action::RENAME) continue; 
            state.OnIntentUpdate(intent, true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) {
        for (auto &[key, intent]: state.intents) {
            if (intent.action != FileIntent::Action::RENAME) continue; 
            state.OnIntentUpdate(intent, false);
        }
    }
    ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("Pending Table", 3, flags)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int row_id  = 0;
        for (auto &[key, intent]: state.intents) {
            if (intent.action != FileIntent::Action::RENAME) continue; 

            ImGui::PushID(row_id++);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool is_active_copy = intent.is_active;
            if (ImGui::Checkbox("##intent_checkbox", &is_active_copy)) {
                state.OnIntentUpdate(intent, is_active_copy);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped(intent.src.c_str());
            ImGui::TableSetColumnIndex(2);

            if (ImGui::InputText("###dest path", &intent.dest)) {
                state.is_conflict_table_dirty = true;
            }

            const char *popup_id = "##intent action popup";
            ImGui::SameLine();
            if (ImGui::Selectable("###row popup button", false, ImGuiSelectableFlags_SpanAllColumns)) {
                ImGui::OpenPopup(popup_id);
            }
            RenderFileIntentChange(state, intent, popup_id);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void RenderFilesDelete(FolderDiff &state) {
    if (ImGui::Button("Select all")) {
        for (auto &[key, intent]: state.intents) {
            if (intent.action != FileIntent::Action::DELETE) continue; 
            state.OnIntentUpdate(intent, true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear all")) {
        for (auto &[key, intent]: state.intents) {
            if (intent.action != FileIntent::Action::DELETE) continue; 
            state.OnIntentUpdate(intent, false);
        }
    }
    ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("##delete table", 2, flags)) {
        ImGui::TableSetupColumn("##is active checkbox", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int i = 0;
        for (auto &[key, intent]: state.intents) {
            if (intent.action != FileIntent::Action::DELETE) continue; 
            ImGui::TableNextRow();

            ImGui::PushID(i++);
            ImGui::TableSetColumnIndex(0);

            bool is_active_copy = intent.is_active;
            if (ImGui::Checkbox("##active checkbox", &is_active_copy)) {
                state.OnIntentUpdate(intent, is_active_copy);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped(intent.src.c_str());
            ImGui::SameLine();

            const char *popup_id = "##intent action popup";
            if (ImGui::Selectable("##action popup row", false, ImGuiSelectableFlags_SpanAllColumns)) {
                ImGui::OpenPopup(popup_id);
            }

            RenderFileIntentChange(state, intent, popup_id);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void RenderFilesConflict(FolderDiff &state) {
    state.UpdateConflictTable();
    auto &conflicts = state.conflicts;

    ImGui::BeginChild("Conflict tab");

    ImGuiTableFlags flags = 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | 
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_SizingStretchProp;

    int src_id = 0;
    for (auto &[dest, targets]: conflicts) {
        auto tree_label = fmt::format("{} ({:d})", dest.c_str(), targets.size());

        bool is_open = ImGui::CollapsingHeader(tree_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (is_open && ImGui::BeginTable("##conflict table", 3, flags)) {

            ImGui::TableSetupColumn("##active checkbox column", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto &key: targets) {
                auto &intent = state.intents[key];
                ImGui::PushID(src_id++);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (intent.action == FileIntent::Action::RENAME) {
                    bool is_active_copy = intent.is_active;
                    if (ImGui::Checkbox("##active_check", &is_active_copy)) {
                        state.OnIntentUpdate(intent, is_active_copy);
                    }
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", intent.src.c_str());

                ImGui::TableSetColumnIndex(2);
                if (intent.action == FileIntent::Action::RENAME) {
                    if (ImGui::InputText("###dest path", &intent.dest)) {
                        state.is_conflict_table_dirty = true;
                    }
                } else {
                    ImGui::TextWrapped("%s", intent.dest.c_str());
                }

                auto popup_label = fmt::format("##action popup {}", src_id);
                ImGui::SameLine();
                if (ImGui::Selectable("##action popup", false, ImGuiSelectableFlags_SpanAllColumns)) {
                    ImGui::OpenPopup(popup_label.c_str());
                }

                RenderFileIntentChange(state, intent, popup_label.c_str());

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::Spacing();
    }

    ImGui::EndChild();
}

void RenderInfoPanel(App &main_app, SeriesFolder &folder) {
    RenderCacheInfo(main_app, folder);
    ImGui::Separator();
    RenderErrors(main_app, folder);
}

void RenderCacheInfo(App &main_app, SeriesFolder &folder) {
    std::scoped_lock cache_lock(folder.m_cache_mutex);
    const auto &cache = folder.m_cache;
    const auto &is_cached = folder.m_is_info_cached;
    if (!is_cached) {
        ImGui::TextWrapped("Cache is missing");
        return;
    }

    ImGui::TextWrapped("Cached Info");

    ImGuiTableFlags flags = 
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

    if (ImGui::BeginTable("Series", 2, flags)) {
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Series ID"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%d", cache.series.id);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Name"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", cache.series.name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Status"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", cache.series.status.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Air Date"); 
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", cache.series.air_date.c_str());

        ImGui::EndTable();
    }
}

void RenderErrors(App &main_app, SeriesFolder &folder) {
    std::scoped_lock errors_lock(folder.m_errors_mutex);
    auto &errors = folder.m_errors;

    ImGui::Text("Error List");
    if (ImGui::BeginListBox("##Error List", ImVec2(-1,-1))) {
        
        auto it = errors.begin();
        auto end = errors.end();

        int gid = 0;
        while (it != end) {
            auto &error = *it;

            ImGui::PushID(gid++);

            bool is_pressed = ImGui::Button("X");
            ImGui::SameLine();
            ImGui::TextWrapped(error.c_str());

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

void RenderSeriesSelectModal(App &main_app, SeriesFolder &folder) {
    static const char *modal_title = "Select a series###series selection modal";

    if (ImGui::Button("Select Series")) {
        ImGui::OpenPopup(modal_title);
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    static char search_string[256] = {0};
    const char *buf = (const char *)(&search_string[0]);

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
            for (auto &r: folder.m_search_result) {
                ImGui::TableNextRow();
                ImGui::PushID(sid++);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(r.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(r.air_date.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", r.id);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text(r.status.c_str());
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

void RenderAppErrors(App &main_app) {
    static const char *modal_title = "Application error###app error modal";

    std::scoped_lock error_lock(main_app.m_app_errors_mutex); 
    auto &errors = main_app.m_app_errors;
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
        if (ImGui::BeginListBox("##app error list")) {
            for (const auto &e: errors) {
                ImGui::Text(e.c_str());
            }

            ImGui::EndListBox();
        }
        ImGui::EndPopup();
    }
}

};
