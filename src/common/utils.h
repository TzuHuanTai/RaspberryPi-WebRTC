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

class Utils {
  public:
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
                                const std::string &url, int quality);
    static void WriteJpegImage(Buffer buffer, const std::string &url);
    static int GetVideoDuration(const std::string &filePath);

    static std::string GenerateUuid();
};

class FileInfo {
    std::string root;
    std::string date;
    std::string hour;
    std::string filename;
    std::string extension;

  public:
    FileInfo(const std::string &root, const std::string &extension = "mp4")
        : root(root),
          extension(extension) {
        time_t now = time(0);
        tm *ltm = localtime(&now);

        std::string year = Utils::PrefixZero(1900 + ltm->tm_year, 4);
        std::string month = Utils::PrefixZero(1 + ltm->tm_mon, 2);
        std::string day = Utils::PrefixZero(ltm->tm_mday, 2);
        std::string hour = Utils::PrefixZero(ltm->tm_hour, 2);
        std::string min = Utils::PrefixZero(ltm->tm_min, 2);
        std::string sec = Utils::PrefixZero(ltm->tm_sec, 2);

        std::stringstream filenameStream;
        filenameStream << year << month << day << "_" << hour << min << sec;
        filenameStream >> filename;

        date = year + month + day;
        this->hour = hour;
    }

    std::string GetFullPath() const {
        return std::filesystem::path(root) / std::filesystem::path(date) /
               std::filesystem::path(hour) / (filename + "." + extension);
    }

    std::string GetFolderPath() const {
        return std::filesystem::path(root) / std::filesystem::path(date) /
               std::filesystem::path(hour);
    }
};

#endif // UTILS_
