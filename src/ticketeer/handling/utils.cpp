#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <unordered_map>

#include "ticketeer/core/conf.hpp"

namespace ticketeer::handling::utils {

namespace {

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

} // namespace

int ParsePage(const std::string &value) {
  if (value.empty())
    return 1;
  int page = 0;
  for (const char ch : value) {
    if (ch < '0' || ch > '9')
      return 1;
    if (page > 100000)
      return 1;
    page = page * 10 + (ch - '0');
    if (page > 1000000)
      return 1;
  }
  return page > 0 ? page : 1;
}

int PageCount(const int total_count) {
  return total_count > 0 ? (total_count + TicketPageSize - 1) / TicketPageSize
                         : 1;
}

int ClampPage(const int page, const int page_count) {
  if (page < 1)
    return 1;
  if (page > page_count)
    return page_count;
  return page;
}

std::string NormalizeSortDir(std::string_view dir) {
  return dir == "asc" ? "asc" : "desc";
}

std::string InferMimeType(const std::string &file_name) {
  const auto ext =
      ToLower(std::filesystem::path(file_name).extension().string());
  static const std::unordered_map<std::string, std::string> mime_types = {
      {".txt", "text/plain"},
      {".csv", "text/csv"},
      {".html", "text/html"},
      {".htm", "text/html"},
      {".json", "application/json"},
      {".xml", "application/xml"},
      {".pdf", "application/pdf"},
      {".png", "image/png"},
      {".jpg", "image/jpeg"},
      {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},
      {".webp", "image/webp"},
      {".svg", "image/svg+xml"},
      {".zip", "application/zip"},
      {".doc", "application/msword"},
      {".docx", "application/"
                "vnd.openxmlformats-officedocument.wordprocessingml.document"},
      {".xls", "application/vnd.ms-excel"},
      {".xlsx",
       "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
      {".ppt", "application/vnd.ms-powerpoint"},
      {".pptx",
       "application/"
       "vnd.openxmlformats-officedocument.presentationml.presentation"}};
  const auto it = mime_types.find(ext);
  return it == mime_types.end() ? "application/octet-stream" : it->second;
}

bool WriteFile(const std::filesystem::path &path, std::string_view content) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec)
    return false;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return false;
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  return out.good();
}

std::filesystem::path AttachmentAbsolutePath(const std::string &file_path,
                                             const std::string &file_name) {
  return std::filesystem::current_path() /
         ticketeer::core::conf::settings.UPLOAD_DIR /
         std::filesystem::path(file_path) /
         std::filesystem::path(file_name).filename();
}

} // namespace ticketeer::handling::utils
