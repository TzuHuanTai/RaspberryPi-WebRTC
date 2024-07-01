#ifndef UTILS_
#define UTILS_

#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <sys/statvfs.h>

class Utils {
public:
    static bool CreateFolder(const std::string& folder_path);
    static std::string ToBase64(const std::string &binary_file);
    static std::string ReadFileInBinary(const std::string &file_path);
    static std::string FindLatestJpg(const std::string &directory);
    static bool CheckDriveSpace(const std::string &file_path, unsigned long min_free_space_mb);
};

#endif // UTILS_