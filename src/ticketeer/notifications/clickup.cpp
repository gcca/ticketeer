#include "clickup.hpp"

#include <algorithm>
#include <cstddef>

#include <drogon/HttpClient.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>

#include "ticketeer/core/conf.hpp"

namespace {

using ticketeer::notifications::clickup::Row;

inline constexpr const char *ApiHost = "https://api.clickup.com";
inline constexpr std::size_t QuoteMaxLength = 1500;

[[nodiscard]] inline const char *PriorityEmoji(const std::string &name) {
  if (name == "Crítica")
    return "🔥";
  if (name == "Alta")
    return "🔴";
  if (name == "Media")
    return "🟡";
  if (name == "Baja")
    return "🟢";
  return "⚪";
}

[[nodiscard]] inline std::string Quote(const std::string &body) {
  std::size_t length = std::min(body.size(), QuoteMaxLength);
  while (length > 0 && length < body.size() &&
         (static_cast<unsigned char>(body[length]) & 0xC0) == 0x80)
    --length;

  std::string quoted = "> ";
  for (const char ch : body.substr(0, length)) {
    if (ch == '\r')
      continue;
    if (ch == '\n')
      quoted += "\n> ";
    else
      quoted += ch;
  }
  if (length < body.size())
    quoted += "…";
  return quoted;
}

[[nodiscard]] inline std::string TicketValue(const Row &ticket,
                                             const char *key) {
  const auto it = ticket.find(key);
  return it == ticket.end() ? std::string{} : it->second;
}

} // namespace

namespace ticketeer::notifications::clickup {

void NotifyTicketCreated(const Row &ticket, const std::string &requester_name) {
  const auto &settings = ticketeer::core::conf::settings;
  if (settings.CLICKUP_API_TOKEN.empty() ||
      settings.CLICKUP_WORKSPACEID.empty() ||
      settings.CLICKUP_TICKETEER_CHANNELID.empty()) {
    LOG_DEBUG << "[clickup] notify not configured";
    return;
  }

  const auto ticket_id = TicketValue(ticket, "id");
  const auto priority_name = TicketValue(ticket, "priority_name");
  const auto status_name = TicketValue(ticket, "status_name");
  const auto due_date = TicketValue(ticket, "due_date");
  const auto body = TicketValue(ticket, "body");

  std::string content = "🎫 **Nuevo ticket #" + ticket_id + "**\n\n";
  content += "👤 **Solicitante:** " + requester_name + "\n";
  content += PriorityEmoji(priority_name);
  content += " **Prioridad:** " + priority_name + "\n";
  content += "📌 **Estado:** " + status_name + "\n";
  if (!due_date.empty())
    content += "📅 **Vence:** " + due_date + "\n";
  content += "\n" + Quote(body);

  Json::Value payload;
  payload["type"] = "message";
  payload["content"] = content;
  payload["content_format"] = "text/md";

  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  const auto payload_body = Json::writeString(writer, payload);

  const std::string path = "/api/v3/workspaces/" +
                           settings.CLICKUP_WORKSPACEID + "/chat/channels/" +
                           settings.CLICKUP_TICKETEER_CHANNELID + "/messages";

  auto client = drogon::HttpClient::newHttpClient(ApiHost);
  auto request = drogon::HttpRequest::newHttpRequest();
  request->setMethod(drogon::Post);
  request->setPath(path);
  request->setBody(payload_body);
  request->setCustomContentTypeString("application/json");
  request->addHeader("Authorization", settings.CLICKUP_API_TOKEN);

  client->sendRequest(
      request, [client](drogon::ReqResult result,
                        const drogon::HttpResponsePtr &response) {
        if (result != drogon::ReqResult::Ok) {
          LOG_DEBUG << "[clickup] notify request failed";
          return;
        }
        const int status_code = static_cast<int>(response->getStatusCode());
        if (status_code < 200 || status_code >= 300) {
          const std::string response_body(response->getBody());
          LOG_DEBUG << "[clickup] notify error: status=" << status_code
                    << " body=" << response_body;
        }
      });
}

} // namespace ticketeer::notifications::clickup
