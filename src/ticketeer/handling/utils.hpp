#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ticketeer::handling::utils {

inline constexpr int TicketPageSize = 9;

[[nodiscard]] int ParsePage(const std::string &value);

[[nodiscard]] int PageCount(int total_count);

[[nodiscard]] int ClampPage(int page, int page_count);

[[nodiscard]] std::string NormalizeSortDir(std::string_view dir);

[[nodiscard]] std::string InferMimeType(const std::string &file_name);

[[nodiscard]] bool WriteFile(const std::filesystem::path &path,
                             std::string_view content);

[[nodiscard]] std::filesystem::path
AttachmentAbsolutePath(const std::string &file_path,
                       const std::string &file_name);

} // namespace ticketeer::handling::utils
