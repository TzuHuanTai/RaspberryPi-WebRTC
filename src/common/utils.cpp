#include "common/utils.h"

#include <filesystem>
#include <iostream>

#include <uuid/uuid.h>

#include "common/logging.h"

namespace fs = std::filesystem;

namespace utils {

bool CreateFolder(const std::string &folder_path) {
    if (folder_path.empty()) {
        return false;
    }

    try {
        fs::create_directories(fs::path(folder_path).parent_path());
        fs::create_directory(folder_path);
        DEBUG_PRINT("Directory created: %s", folder_path.c_str());
        return true;
    } catch (const fs::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << folder_path << std::endl;
        std::cerr << e.what() << std::endl;
        return false;
    }
}

std::string GenerateUuid() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

} // namespace utils
