#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr const char *kHost = "127.0.0.1";
constexpr int kPort = 5521;
constexpr int kTimeoutSeconds = 3;

bool Fetch(std::string &response) {
  int const fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;

  timeval const timeout{.tv_sec = kTimeoutSeconds, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(kPort);
  if (inet_pton(AF_INET, kHost, &addr.sin_addr) != 1) {
    close(fd);
    return false;
  }

  if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return false;
  }

  std::string const request =
      std::string("GET /ticketeer/healthcheck HTTP/1.1\r\nHost: ") + kHost +
      "\r\nConnection: close\r\n\r\n";
  if (send(fd, request.data(), request.size(), 0) < 0) {
    close(fd);
    return false;
  }

  char buffer[512];
  ssize_t n;
  while ((n = recv(fd, buffer, sizeof(buffer), 0)) > 0) {
    response.append(buffer, static_cast<size_t>(n));
  }

  close(fd);
  return true;
}

} // namespace

int main() {
  std::string response;
  if (!Fetch(response))
    return 1;

  return response.starts_with("HTTP/1.1 200") ||
                 response.starts_with("HTTP/1.0 200")
             ? 0
             : 1;
}
