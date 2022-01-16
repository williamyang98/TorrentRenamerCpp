#include <string>
#include <mutex>

#include <imgui.h>
#include <fmt/format.h>

#include "gui_ext.h"
#include "font_awesome_definitions.h"

namespace app::gui 
{

static void RenderSeriesList(App &main_app);
static void RenderSeriesFolder(App &main_app, SeriesFolder &folder);
static void RenderEpisodes(App &main_app, SeriesFolder &folder);
static void RenderConflicts(ConflictFiles &conflicts);
static void RenderInfoPanel(App &main_app, SeriesFolder &folder);
static void RenderSeriesSelectModal(App &main_app, SeriesFolder &folder);

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
        return { ICON_FA_EXCLAMATION_TRIANGLE, ImColor(255,255,0) };
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
    const int busy_count = main_app.m_global_busy_count;
    ImGui::BeginDisabled(busy_count > 0);
    if (ImGui::Button("Refresh project structure")) {
        main_app.refresh_folders();
    }

    if (ImGui::Button("Scan contents of all folders")) {
        for (auto &folder: main_app.m_folders) {
            main_app.m_thread_pool.push([&folder](int pid) {
                folder.update_state();
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
    if (ImGui::BeginListBox("series_list_box", ImVec2(-1,-1))) {

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
                main_app.m_thread_pool.push([&folder](int pid) {
                    folder.update_state();
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

void RenderEpisodes(App &main_app, SeriesFolder &folder) {
    bool is_busy = folder.m_is_busy;

    ImGui::BeginDisabled(is_busy);

    if (ImGui::Button("Scan for changes")) {
        main_app.m_thread_pool.push([&folder](int pid) {
            folder.update_state();
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh from cache")) {
        main_app.m_thread_pool.push([&folder](int pid) {
            folder.load_cache();
            folder.update_state();
        });
    }

    ImGui::SameLine();
    if (ImGui::Button("Download and refresh from tvdb")) {
        main_app.m_thread_pool.push([&folder, &main_app](int pid) {
            folder.refresh_cache(folder.m_cache.series.id, main_app.m_token.c_str());
            folder.update_state();
        });
    }

    ImGui::SameLine();
    RenderSeriesSelectModal(main_app, folder);

    ImGui::EndDisabled();

    // render the state tree
    auto &state = folder.m_state;
    std::scoped_lock state_lock(folder.m_state_mutex);

    if (ImGui::BeginTabBar("Status")) {
        auto tab_name = fmt::format("Completed {:d}", state.completed.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            if (ImGui::BeginListBox("Completed", ImVec2(-1,-1))) {
                for (const auto &e: state.completed) {
                    ImGui::TextWrapped(e.c_str());
                }
                ImGui::EndListBox();
            }

            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Ignores {:d}", state.ignores.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            if (ImGui::BeginListBox("Ignore", ImVec2(-1,-1))) {
                for (const auto &e: state.ignores) {
                    ImGui::TextWrapped(e.c_str());
                }
                ImGui::EndListBox();
            }

            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Pending {:d}", state.pending.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("Pending Table", 3, flags)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (auto &e: state.pending) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Checkbox("", &e.active);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextWrapped(e.target.c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextWrapped(e.dest.c_str());
                }

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Deletes {:d}", state.deletes.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            if (ImGui::BeginListBox("Deletes", ImVec2(-1,-1))) {
                for (auto &e: state.deletes) {
                    ImGui::Checkbox(e.filename.c_str(), &e.active);
                }
                ImGui::EndListBox();
            }
            ImGui::EndTabItem();
        }

        tab_name = fmt::format("Conflicts {:d}", state.conflicts.size());
        if (ImGui::BeginTabItem(tab_name.c_str())) {
            RenderConflicts(state.conflicts);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

}

void RenderConflicts(ConflictFiles &conflicts) {
    ImGuiTableFlags flags = 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | 
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_SizingStretchProp;

    for (auto &[dest, targets]: conflicts) {
        auto tree_label = fmt::format("{} ({:d})", dest.c_str(), targets.size());
        bool is_open = ImGui::CollapsingHeader(tree_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (is_open && ImGui::BeginTable("Table", 3, flags)) {

            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Destination", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto &e: targets) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Checkbox("", &e.active);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped(e.target.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped(e.dest.c_str());
            }

            ImGui::EndTable();
        }
    }
}

static void RenderCacheInfo(App &main_app, SeriesFolder &folder);
static void RenderErrors(App &main_app, SeriesFolder &folder);

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
        ImGui::TextWrapped("%s", cache.series.air_date.c_str());

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
    if (ImGui::BeginListBox("Error List", ImVec2(-1,-1))) {
        
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
    static const char *modal_title = "Series Selection Modal";

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
            main_app.m_thread_pool.push([&folder, buf, &main_app](int pid) {
                folder.search_series(buf, main_app.m_token.c_str());
            });
        }
        ImGui::Separator();

        ImGuiTableFlags flags = 
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | 
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable |
            ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;

        if (ImGui::BeginTable("Search Results", 5, flags)) {
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
                    main_app.m_thread_pool.push([id, &folder, &main_app](int pid) {
                        folder.refresh_cache(id, main_app.m_token.c_str());
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

};
