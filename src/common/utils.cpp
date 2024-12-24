#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <sys/statvfs.h>
#include <uuid/uuid.h>

extern "C" {
#include <libavformat/avformat.h>
}
#include <jpeglib.h>
#include <third_party/libyuv/include/libyuv.h>

#include "common/logging.h"
#include "common/utils.h"

bool Utils::CreateFolder(const std::string &folder_path) {
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

void Utils::RotateFiles(const std::string &folder_path) {
    std::vector<fs::path> date_folders;

    for (const auto &entry : fs::directory_iterator(folder_path)) {
        if (entry.is_directory()) {
            date_folders.push_back(entry.path());
        }
    }

    if (!date_folders.empty()) {
        std::sort(date_folders.begin(), date_folders.end());
        fs::path oldest_date_folder = date_folders.front();
        std::vector<fs::path> hour_folders;

        for (const auto &hour_entry : fs::directory_iterator(oldest_date_folder)) {
            if (hour_entry.is_directory()) {
                hour_folders.push_back(hour_entry.path());
            }
        }

        if (!hour_folders.empty()) {
            std::sort(hour_folders.begin(), hour_folders.end());
            fs::path oldest_hour_folder = hour_folders.front();

            std::vector<fs::path> mp4_files;

            for (const auto &file_entry : fs::directory_iterator(oldest_hour_folder)) {
                if (file_entry.is_regular_file() && file_entry.path().extension() == ".mp4") {
                    mp4_files.push_back(file_entry.path());
                }
            }

            if (!mp4_files.empty()) {
                std::sort(mp4_files.begin(), mp4_files.end(),
                          [](const fs::path &a, const fs::path &b) {
                              return fs::last_write_time(a) < fs::last_write_time(b);
                          });
                fs::remove(mp4_files.front());
                INFO_PRINT("Deleted oldest .mp4 file: %s", mp4_files.front().string().c_str());

                fs::path corresponding_image = mp4_files.front().replace_extension(".jpg");
                if (fs::exists(corresponding_image)) {
                    fs::remove(corresponding_image);
                    INFO_PRINT("Deleted corresponding .jpg file: %s",
                               corresponding_image.string().c_str());
                }

                if (fs::is_empty(oldest_hour_folder)) {
                    fs::remove(oldest_hour_folder);
                    INFO_PRINT("Deleted empty hour folder: %s",
                               oldest_hour_folder.string().c_str());

                    if (fs::is_empty(oldest_date_folder)) {
                        fs::remove(oldest_date_folder);
                        INFO_PRINT("Deleted empty date folder: %s",
                                   oldest_date_folder.string().c_str());
                    }
                }
            } else {
                fs::remove_all(oldest_hour_folder);
                INFO_PRINT("Deleted empty hour folder: %s", oldest_hour_folder.string().c_str());
            }
        }
    }
}

std::string Utils::ToBase64(const std::string &binary_file) {
    std::string out;
    int val = 0, valb = -6;
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (unsigned char c : binary_file) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

std::string Utils::ReadFileInBinary(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::vector<std::pair<fs::file_time_type, fs::path>> Utils::GetFiles(const std::string &path,
                                                                     const std::string &extension) {
    std::vector<std::pair<fs::file_time_type, fs::path>> files;
    for (const auto &entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            files.emplace_back(fs::last_write_time(entry), entry.path());
        }
    }
    return files;
}

std::string Utils::FindLatestSubDir(const std::string &path) {
    std::string latestDir;
    for (const auto &entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            if (entry.path().filename().string() > latestDir) {
                latestDir = entry.path().filename().string();
            }
        }
    }
    return latestDir;
}

std::string Utils::GetPreviousDate(const std::string &dateStr) {
    std::tm tm = {};
    std::istringstream ss(dateStr);
    ss >> std::get_time(&tm, "%Y%m%d");

    tm.tm_mday -= 1;
    mktime(&tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d");

    return oss.str();
}

std::string Utils::FindLatestFile(const std::string &path, const std::string &extension) {
    std::string latestDateDir = Utils::FindLatestSubDir(path);
    if (latestDateDir.empty()) {
        std::cerr << "No date directories found." << std::endl;
        return "";
    }

    std::string datePath = path + "/" + latestDateDir;
    std::string latestHourDir = Utils::FindLatestSubDir(datePath);
    if (latestHourDir.empty()) {
        std::cerr << "No hour directories found." << std::endl;
        return "";
    }

    std::string latestDir = datePath + "/" + latestHourDir;
    auto files = Utils::GetFiles(latestDir, extension);
    std::sort(files.begin(), files.end(), std::greater<>());

    return files[0].second.string();
}

std::string Utils::FindSecondNewestFile(const std::string &path, const std::string &extension) {
    std::string latestDateDir = Utils::FindLatestSubDir(path);
    if (latestDateDir.empty()) {
        std::cerr << "No date directories found." << std::endl;
        return "";
    }

    std::string datePath = path + "/" + latestDateDir;
    std::string latestHourDir = Utils::FindLatestSubDir(datePath);
    if (latestHourDir.empty()) {
        std::cerr << "No hour directories found." << std::endl;
        return "";
    }

    std::string latestDir = datePath + "/" + latestHourDir;
    auto files = Utils::GetFiles(latestDir, extension);

    // find previous hour
    if (files.size() < 2) {
        std::string prevHourDir = latestHourDir;
        if (!prevHourDir.empty() && prevHourDir > "00") {
            prevHourDir = std::to_string(std::stoi(prevHourDir) - 1);
            if (prevHourDir.length() < 2) {
                prevHourDir = "0" + prevHourDir;
            }
            std::string prevDir = datePath + "/" + prevHourDir;
            auto prevFiles = Utils::GetFiles(prevDir, extension);

            files.insert(files.end(), prevFiles.begin(), prevFiles.end());
        }
    }

    // find previous date
    if (files.size() < 2) {
        std::string prevDateDir = Utils::GetPreviousDate(latestDateDir);

        std::string prevDatePath = path + "/" + prevDateDir;
        latestHourDir = Utils::FindLatestSubDir(prevDatePath);
        std::string prevDir = prevDatePath + "/" + latestHourDir;
        auto prevFiles = Utils::GetFiles(prevDir, extension);

        files.insert(files.end(), prevFiles.begin(), prevFiles.end());
    }

    std::sort(files.begin(), files.end(), std::greater<>());

    if (files.size() < 2) {
        std::cerr << "Not enough files to determine the second newest file." << std::endl;
        return "";
    }

    return files[1].second.string();
}

std::chrono::system_clock::time_point Utils::ParseDatetime(const std::string &datetime_str) {
    std::tm tm = {};
    std::stringstream ss(datetime_str);
    ss >> std::get_time(&tm, "%Y%m%d_%H%M%S");
    tm.tm_isdst = -1;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string Utils::FindFilesFromDatetime(const std::string &root, const std::string &basename) {
    if (basename.length() < 15) {
        return "";
    }

    std::string date = basename.substr(0, 8);
    std::string time = basename.substr(9);
    std::string hour = time.substr(0, 2);

    fs::path hour_path(root + date + "/" + hour);

    if (!fs::exists(hour_path)) {
        return "";
    }

    auto time_limit = ParseDatetime(basename);

    auto files = GetFiles(hour_path.string(), ".mp4");
    std::sort(files.begin(), files.end(), std::greater<>());

    int max_searching_folder = 10;
    for (int count = 0; count < max_searching_folder; count++) {
        // find in the same hour
        auto files = GetFiles(hour_path.string(), ".mp4");
        std::sort(files.begin(), files.end(), std::greater<>());

        for (auto &p : files) {
            if (fs::file_time_type::clock::to_sys(p.first) < time_limit) {
                return p.second.string();
            }
        }

        fs::path date_path = hour_path.parent_path();
        if (hour > "00") {
            // update hour path to previous hour
            auto prev_hour = std::to_string(std::stoi(hour) - 1);
            if (prev_hour.length() < 2) {
                prev_hour = "0" + prev_hour;
            }
            hour_path = date_path / prev_hour;
            if (!fs::exists(hour_path)) {
                ERROR_PRINT("pre hour path %s is not found", hour_path.string().c_str());
                break;
            }
        } else {
            // update date path to previous date
            fs::path root_path = date_path.parent_path();
            std::string date = date_path.filename();
            auto prev_date = GetPreviousDate(date);
            date_path = root_path / prev_date;
            hour_path = date_path / "23";
            if (!fs::exists(date_path)) {
                ERROR_PRINT("pre date path %s is not found", date_path.string().c_str());
                break;
            }
        }
    }

    return "";
}

std::vector<std::string> Utils::FindOlderFiles(const std::string &file_path, int request_num) {
    std::vector<std::string> result;
    fs::path file(file_path);
    if (!fs::exists(file)) {
        return result;
    }
    auto file_last_write_time = fs::last_write_time(file);
    auto extension = file.extension();

    fs::path hour_path = file.parent_path();
    fs::path date_path = hour_path.parent_path();
    fs::path root_path = date_path.parent_path();

    while (result.size() < request_num) {
        // find in the same hour
        auto files = GetFiles(hour_path.string(), extension.string());
        std::sort(files.begin(), files.end(), std::greater<>());

        for (auto &p : files) {
            if (p.first < file_last_write_time) {
                result.push_back(p.second.string());
                if (result.size() == request_num) {
                    return result;
                }
            }
        }

        std::string hour = hour_path.filename();
        if (hour > "00") {
            // update hour path to previous hour
            auto prev_hour = std::to_string(std::stoi(hour) - 1);
            if (prev_hour.length() < 2) {
                prev_hour = "0" + prev_hour;
            }
            hour_path = date_path / prev_hour;
            if (!fs::exists(hour_path)) {
                ERROR_PRINT("pre hour path %s is not found", hour_path.string().c_str());
                break;
            }
        } else {
            // update date path to previous date
            std::string date = date_path.filename();
            auto prev_date = GetPreviousDate(date);
            date_path = root_path / prev_date;
            hour_path = date_path / "23";
            if (!fs::exists(date_path)) {
                ERROR_PRINT("pre date path %s is not found", date_path.string().c_str());
                break;
            }
        }
    }

    return result;
}

bool Utils::CheckDriveSpace(const std::string &file_path, unsigned long min_free_byte) {
    struct statvfs stat;
    if (statvfs(file_path.c_str(), &stat) != 0) {
        return false;
    }
    return (stat.f_bsize * stat.f_bavail) >= min_free_byte;
}

std::string Utils::PrefixZero(int src, int digits) {
    std::string str = std::to_string(src);
    std::string n_zero(digits - str.length(), '0');
    return n_zero + str;
}

FileInfo Utils::GenerateFilename() {
    time_t now = time(0);
    tm *ltm = localtime(&now);

    std::string year = Utils::PrefixZero(1900 + ltm->tm_year, 4);
    std::string month = Utils::PrefixZero(1 + ltm->tm_mon, 2);
    std::string day = Utils::PrefixZero(ltm->tm_mday, 2);
    std::string hour = Utils::PrefixZero(ltm->tm_hour, 2);
    std::string min = Utils::PrefixZero(ltm->tm_min, 2);
    std::string sec = Utils::PrefixZero(ltm->tm_sec, 2);

    std::stringstream s1;
    std::string filename;
    s1 << year << month << day << "_" << hour << min << sec;
    s1 >> filename;

    FileInfo info = {.date = year + month + day, .hour = hour, .filename = filename};

    return info;
}

Buffer Utils::ConvertYuvToJpeg(const uint8_t *yuv_data, int width, int height, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    Buffer jpegBuffer;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    uint8_t *data = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&cinfo, &data, &size);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_EXT_BGR;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = width * 3;
    uint8_t *rgb_data = (uint8_t *)malloc(width * height * 3);
    libyuv::I420ToRGB24(yuv_data, width, yuv_data + width * height, width / 2,
                        yuv_data + width * height + (width * height / 4), width / 2, rgb_data,
                        width * 3, width, height);

    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &rgb_data[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    free(rgb_data);

    jpegBuffer.start = std::unique_ptr<uint8_t, FreeDeleter>(data);
    jpegBuffer.length = size;

    return jpegBuffer;
}

void Utils::WriteJpegImage(Buffer buffer, const std::string &url) {
    FILE *file = fopen(url.c_str(), "wb");
    if (file) {
        fwrite((uint8_t *)buffer.start.get(), 1, buffer.length, file);
        fclose(file);
        DEBUG_PRINT("JPEG data successfully written to %s", url.c_str());
    } else {
        ERROR_PRINT("Failed to open file for writing: %s", url.c_str());
    }
}

void Utils::CreateJpegImage(const uint8_t *yuv_data, int width, int height,
                            const std::string &url) {
    try {
        auto jpg_buffer = Utils::ConvertYuvToJpeg(yuv_data, width, height, 30);
        WriteJpegImage(std::move(jpg_buffer), url);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int Utils::GetVideoDuration(const std::string &filePath) {
    AVFormatContext *formatContext = nullptr;
    if (avformat_open_input(&formatContext, filePath.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open file: " << filePath << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        avformat_close_input(&formatContext);
        return -1;
    }

    if (!formatContext) {
        std::cerr << "Invalid format context!" << std::endl;
        return -1;
    }

    int64_t duration = formatContext->duration;
    int durationInSeconds = static_cast<int>(duration / AV_TIME_BASE);

    avformat_close_input(&formatContext);

    return durationInSeconds;
}

std::string Utils::GenerateUuid() {
    uuid_t uuid;
    char uuid_str[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}
