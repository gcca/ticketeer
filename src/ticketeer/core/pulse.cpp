#include "pulse.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include <poll.h>
#include <spawn.h>
#include <sqlite3.h>
#include <sys/wait.h>
#include <trantor/utils/Logger.h>
#include <unistd.h>

#include "ticketeer/core/conf.hpp"

extern "C" char **environ;

namespace ticketeer::core::pulse {

namespace {

inline constexpr std::size_t ReadBufferSize = 4096;
inline constexpr std::chrono::seconds JobTimeout = std::chrono::minutes(5);
inline constexpr std::chrono::seconds KillGrace{5};
inline constexpr std::chrono::milliseconds ReapInterval{50};

struct Job {
  std::string name;
  std::string arg;
  pid_t pid = -1;
  int out_fd = -1;
  int err_fd = -1;
  std::string output;
  std::string error;
  int status = 0;
};

void TrimTrailingNewlines(std::string &text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
    text.pop_back();
}

[[nodiscard]] Job Run(const std::string &name, const std::string &arg) {
  Job job{.name = name, .arg = arg};

  std::vector<std::string> args;
  for (const auto token : std::views::split(arg, ' ')) {
    if (!token.empty())
      args.emplace_back(token.begin(), token.end());
  }

  std::vector<char *> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(const_cast<char *>(name.c_str()));
  for (const auto &token : args)
    argv.push_back(const_cast<char *>(token.c_str()));
  argv.push_back(nullptr);

  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};
  if (pipe(out_pipe) != 0) {
    LOG_ERROR << "[pulse] pipe error " << name << ": " << std::strerror(errno);
    return job;
  }
  if (pipe(err_pipe) != 0) {
    LOG_ERROR << "[pulse] pipe error " << name << ": " << std::strerror(errno);
    close(out_pipe[0]);
    close(out_pipe[1]);
    return job;
  }

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
  posix_spawn_file_actions_addclose(&actions, out_pipe[1]);
  posix_spawn_file_actions_addclose(&actions, err_pipe[0]);
  posix_spawn_file_actions_addclose(&actions, err_pipe[1]);

  pid_t pid = 0;
  const int rc =
      posix_spawnp(&pid, name.c_str(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(out_pipe[1]);
  close(err_pipe[1]);

  if (rc != 0) {
    close(out_pipe[0]);
    close(err_pipe[0]);
    LOG_ERROR << "[pulse] spawn error " << name << ": " << std::strerror(rc);
    return job;
  }

  job.pid = pid;
  job.out_fd = out_pipe[0];
  job.err_fd = err_pipe[0];
  return job;
}

[[nodiscard]] bool Reap(Job &job,
                        std::chrono::steady_clock::time_point deadline) {
  for (;;) {
    const pid_t reaped = waitpid(job.pid, &job.status, WNOHANG);
    if (reaped == job.pid)
      return true;
    if (reaped < 0) {
      if (errno == EINTR)
        continue;
      LOG_ERROR << "[pulse] wait error " << job.name << ": "
                << std::strerror(errno);
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline)
      return false;
    std::this_thread::sleep_for(ReapInterval);
  }
}

void Wait(Job &job) {
  if (job.pid < 0)
    return;

  const auto deadline = std::chrono::steady_clock::now() + JobTimeout;

  pollfd fds[2] = {{job.out_fd, POLLIN, 0}, {job.err_fd, POLLIN, 0}};
  std::string *sinks[2] = {&job.output, &job.error};

  const auto buffer = std::make_unique<char[]>(ReadBufferSize);

  bool timed_out = false;

  for (int open_count = 2; open_count > 0;) {
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
    if (remaining <= std::chrono::milliseconds::zero()) {
      timed_out = true;
      break;
    }

    const int ready = poll(fds, 2, static_cast<int>(remaining.count()));
    if (ready == 0) {
      timed_out = true;
      break;
    }
    if (ready < 0) {
      if (errno == EINTR)
        continue;
      LOG_ERROR << "[pulse] poll error " << job.name << ": "
                << std::strerror(errno);
      break;
    }

    for (int i = 0; i < 2; ++i) {
      if (fds[i].fd < 0 || fds[i].revents == 0)
        continue;

      const ssize_t count = read(fds[i].fd, buffer.get(), ReadBufferSize);
      if (count > 0) {
        sinks[i]->append(buffer.get(), static_cast<std::size_t>(count));
        continue;
      }
      if (count < 0 && (errno == EINTR || errno == EAGAIN))
        continue;

      fds[i].fd = -1;
      --open_count;
    }
  }

  close(job.out_fd);
  close(job.err_fd);
  job.out_fd = -1;
  job.err_fd = -1;

  if (!timed_out)
    timed_out = !Reap(job, deadline);

  if (timed_out) {
    LOG_ERROR << "[pulse] timeout " << job.name << (job.arg.empty() ? "" : " ")
              << job.arg << ", killing pid " << job.pid << " after "
              << JobTimeout.count() << "s";
    if (kill(job.pid, SIGKILL) != 0)
      LOG_ERROR << "[pulse] kill error " << job.name << ": "
                << std::strerror(errno);
    else if (!Reap(job, std::chrono::steady_clock::now() + KillGrace))
      LOG_ERROR << "[pulse] unreaped " << job.name << ", pid " << job.pid;
  }

  job.pid = -1;

  TrimTrailingNewlines(job.output);
  TrimTrailingNewlines(job.error);

  if (!timed_out && WIFEXITED(job.status) && WEXITSTATUS(job.status) == 0) {
    LOG_INFO << "[pulse] ran " << job.name << (job.arg.empty() ? "" : " ")
             << job.arg << (job.output.empty() ? "" : "\n") << job.output;
  } else {
    LOG_ERROR << "[pulse] failed " << job.name << (job.arg.empty() ? "" : " ")
              << job.arg << ", status=" << job.status
              << (job.output.empty() ? "" : "\n") << job.output;
  }

  if (!job.error.empty())
    LOG_ERROR << "[pulse] stderr " << job.name << (job.arg.empty() ? "" : " ")
              << job.arg << "\n"
              << job.error;
}

void Loop() {
  const std::string &db_path = conf::settings.PULSE_DB_PATH;

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) !=
      SQLITE_OK) {
    LOG_ERROR << "[pulse] open error: " << sqlite3_errmsg(db);
    sqlite3_close(db);
    return;
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql = "SELECT name, arg FROM command WHERE enabled = TRUE";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR << "[pulse] query error: " << sqlite3_errmsg(db);
    sqlite3_close(db);
    return;
  }

  std::vector<Job> jobs;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto name =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    const auto arg =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    if (name)
      jobs.push_back(Run(name, arg ? arg : ""));
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  for (auto &job : jobs)
    Wait(job);
}

[[nodiscard]] bool Ready() {
  const std::string &db_path = conf::settings.PULSE_DB_PATH;

  if (!std::filesystem::exists(std::filesystem::path(db_path)))
    return false;

  sqlite3 *db = nullptr;
  if (sqlite3_open_v2(db_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) !=
      SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  sqlite3_stmt *stmt = nullptr;
  const char *sql =
      "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'command'";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  const bool ready = sqlite3_step(stmt) == SQLITE_ROW;

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ready;
}

} // namespace

void Start() {
  if (!Ready())
    return;

  LOG_INFO << "[pulse] starting, interval="
           << conf::settings.PULSE_INTERVAL.count() << "s";

  std::thread([] {
    for (;;) {
      std::this_thread::sleep_for(conf::settings.PULSE_INTERVAL);
      Loop();
    }
  }).detach();
}

} // namespace ticketeer::core::pulse
