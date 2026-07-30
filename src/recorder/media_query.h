#ifndef RECORDER_MEDIA_QUERY_H_
#define RECORDER_MEDIA_QUERY_H_

#include <cstdint>
#include <string>
#include <vector>

namespace media_query {

std::string FindLatestCompleteFile(const std::string &base_dir, const std::string &extension);
std::vector<std::string> FindOlderFiles(const std::string &base_dir, const std::string &file_path,
                                        int request_num);
std::string FindFilesFromDatetime(const std::string &root, const std::string &basename);
uint32_t GetVideoDuration(const std::string &file_path);
std::string GetThumbnailBase64(const std::string &file_path, int scale_denom = 8, int quality = 75);

} // namespace media_query

#endif // RECORDER_MEDIA_QUERY_H_
