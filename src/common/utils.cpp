#include "common/utils.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include <jpeglib.h>
#include <third_party/libyuv/include/libyuv.h>

#include "common/logging.h"

namespace fs = std::filesystem;

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

void Utils::RotateFiles(std::string folder_path) {
    std::vector<fs::path> date_folders;

    for (const auto &entry : fs::directory_iterator(folder_path)) {
        if (entry.is_directory()) {
            date_folders.push_back(entry.path());
        }
    }
    std::sort(date_folders.begin(), date_folders.end());

    if (!date_folders.empty()) {
        fs::path oldest_date_folder = date_folders.front();
        std::vector<fs::path> hour_folders;

        for (const auto &hour_entry : fs::directory_iterator(oldest_date_folder)) {
            if (hour_entry.is_directory()) {
                hour_folders.push_back(hour_entry.path());
            }
        }

        std::sort(hour_folders.begin(), hour_folders.end());

        if (!hour_folders.empty()) {
            fs::path oldest_hour_folder = hour_folders.front();

            std::vector<fs::path> mp4_files;

            for (const auto &file_entry : fs::directory_iterator(oldest_hour_folder)) {
                if (file_entry.is_regular_file() && file_entry.path().extension() == ".mp4") {
                    mp4_files.push_back(file_entry.path());
                }
            }

            std::sort(mp4_files.begin(), mp4_files.end(), [](const fs::path &a, const fs::path &b) {
                return fs::last_write_time(a) < fs::last_write_time(b);
            });

            if (!mp4_files.empty()) {
                fs::remove(mp4_files.front());
                std::cout << "Deleted oldest .mp4 file: " << mp4_files.front() << std::endl;

                // Also delete corresponding .jpg file
                fs::path corresponding_image = mp4_files.front().replace_extension(".jpg");
                if (fs::exists(corresponding_image)) {
                    fs::remove(corresponding_image);
                    std::cout << "Deleted corresponding .jpg file: " << corresponding_image
                              << std::endl;
                }

                if (fs::is_empty(oldest_hour_folder)) {
                    fs::remove(oldest_hour_folder);
                    std::cout << "Deleted empty hour folder: " << oldest_hour_folder << std::endl;

                    if (fs::is_empty(oldest_date_folder)) {
                        fs::remove(oldest_date_folder);
                        std::cout << "Deleted empty date folder: " << oldest_date_folder
                                  << std::endl;
                    }
                }
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

std::string Utils::FindLatestJpg(const std::string &directory) {
    std::string latest_file;
    auto latest_time = std::filesystem::file_time_type::min();

    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".jpg") {
            auto file_time = std::filesystem::last_write_time(entry);
            if (latest_file.empty() || file_time > latest_time) {
                latest_time = file_time;
                latest_file = entry.path().string();
            }
        }
    }

    if (latest_file.empty()) {
        throw std::runtime_error("No jpg files found in the directory");
    }

    return latest_file;
}

bool Utils::CheckDriveSpace(const std::string &file_path, unsigned long min_free_space_mb) {
    struct statvfs stat;
    if (statvfs(file_path.c_str(), &stat) != 0) {
        return false;
    }
    unsigned long free_space = stat.f_bsize * stat.f_bavail;
    unsigned long free_space_mb = free_space / (1024 * 1024);

    return free_space_mb >= min_free_space_mb;
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

void Utils::WriteJpegImage(Buffer buffer, std::string url) {
    FILE *file = fopen(url.c_str(), "wb");
    if (file) {
        fwrite((uint8_t *)buffer.start.get(), 1, buffer.length, file);
        fclose(file);
        DEBUG_PRINT("JPEG data successfully written to %s", url.c_str());
    } else {
        ERROR_PRINT("Failed to open file for writing: %s", url.c_str());
    }
}

void Utils::CreateJpegImage(const uint8_t *yuv_data, int width, int height, std::string url) {
    try {
        auto jpg_buffer = Utils::ConvertYuvToJpeg(yuv_data, width, height, 30);
        WriteJpegImage(std::move(jpg_buffer), url);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
