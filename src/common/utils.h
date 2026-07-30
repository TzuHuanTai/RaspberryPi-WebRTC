#ifndef COMMON_UTILS_H_
#define COMMON_UTILS_H_

#include <string>

namespace utils {

bool CreateFolder(const std::string &folder_path);
std::string GenerateUuid();

} // namespace utils

#endif // COMMON_UTILS_H_
