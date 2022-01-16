#include <filesystem>
#include <iostream>

#include "renamer.h"

namespace app 
{

namespace fs = std::filesystem;

bool rename_series_directory(const fs::path &root, SeriesState &state) {
    // TODO: Add checking and error handling for invalid states
    bool all_success = true;

    for (auto &e: state.pending) {
        if (!e.active) {
            continue;
        }

        auto src_path = root / e.target;
        auto dest_path = root / e.dest;

        try {
            fs::rename(src_path, dest_path);
        } catch (fs::filesystem_error &ex) {
            all_success = false;
            std::cerr << 
                "Failed to rename pending file (" << src_path <<") to" << 
                "(" << dest_path << "): " << ex.what() << std::endl;
        } 
    }

    for (auto &e: state.deletes) {
        if (!e.active) {
            continue;
        }

        auto src_path = root / e.filename;

        try {
            fs::remove(src_path);
        } catch (fs::filesystem_error &ex) {
            all_success = false;
            std::cerr << "Failed to remove file (" << src_path << "): " <<
            ex.what() << std::endl;
        }
    }

    for (auto &[conflict_dest, targets]: state.conflicts) {
        for (auto &e: targets) {
            if (!e.active) {
                continue;
            }

            auto src_path = root / e.target;
            auto dest_path = root / e.dest;
            try {
                fs::rename(src_path, dest_path);
            } catch (fs::filesystem_error &ex) {
                all_success = false;
                std::cerr << 
                    "Failed to rename conflicting file (" << src_path <<") to" << 
                    "(" << dest_path << ") conflicting with (" << (root / conflict_dest) << "): " << ex.what() << std::endl;
            } 
        }
    }

    return all_success;
}

};