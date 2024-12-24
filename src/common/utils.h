#ifndef UTILS_
#define UTILS_

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

struct FreeDeleter {
    void operator()(uint8_t *ptr) const {
        if (ptr) {
            free(ptr);
        }
        ptr = nullptr;
    }
};

struct Buffer {
    std::unique_ptr<uint8_t, FreeDeleter> start;
    unsigned long length;
};

struct FileInfo {
    std::string date;
    std::string hour;
    std::string filename;
};

class Utils {
  public:
    static FileInfo GenerateFilename();
    static std::string PrefixZero(int src, int digits);
    static std::string ToBase64(const std::string &binary_file);
    static std::string ReadFileInBinary(const std::string &file_path);
    static std::vector<std::pair<fs::file_time_type, fs::path>>
    GetFiles(const std::string &path, const std::string &extension);
    static std::string FindLatestSubDir(const std::string &path);
    static std::string GetPreviousDate(const std::string &dateStr);
    static std::string FindLatestFile(const std::string &path, const std::string &extension);
    static std::string FindSecondNewestFile(const std::string &path, const std::string &extension);
    static std::chrono::system_clock::time_point ParseDatetime(const std::string &datetime_str);
    static std::string FindFilesFromDatetime(const std::string &root, const std::string &basename);
    static std::vector<std::string> FindOlderFiles(const std::string &file_path, int request_num);

    static bool CreateFolder(const std::string &folder_path);
    static void RotateFiles(const std::string &folder_path);
    static bool CheckDriveSpace(const std::string &file_path, unsigned long min_free_byte);
    static Buffer ConvertYuvToJpeg(const uint8_t *yuv_data, int width, int height,
                                   int quality = 100);
    static void CreateJpegImage(const uint8_t *yuv_data, int width, int height,
                                const std::string &url);
    static void WriteJpegImage(Buffer buffer, const std::string &url);
    static int GetVideoDuration(const std::string &filePath);

    static std::string GenerateUuid();
};

#endif // UTILS_
