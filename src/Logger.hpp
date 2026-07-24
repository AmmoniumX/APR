#pragma once

#include <chrono>
#include <format>
#include <iostream>
#include <utility>

namespace App {
inline constexpr auto PROGRAM_NAME = "APRI";
class Logger {
public:
  enum class LogLevel : int8_t {
    NONE = 0,
    ERROR = 1,
    WARN = 2,
    INFO = 3,
    DEBUG = 4,
    ALL = 5
  };

  explicit Logger(LogLevel level) : level_{level} {}

  LogLevel level() const { return level_; }

  template <class... Args>
  void debug(std::format_string<Args...> fmt, Args &&...args) {
    if (level_ >= LogLevel::DEBUG) {
      println("DEBUG", fmt, std::forward<Args>(args)...);
    }
  }

  template <class... Args>
  void info(std::format_string<Args...> fmt, Args &&...args) {
    if (level_ >= LogLevel::INFO) {
      println("INFO", fmt, std::forward<Args>(args)...);
    }
  }

  template <class... Args>
  void warn(std::format_string<Args...> fmt, Args &&...args) {
    if (level_ >= LogLevel::WARN) {
      println("WARN", fmt, std::forward<Args>(args)...);
    }
  }

  template <class... Args>
  void error(std::format_string<Args...> fmt, Args &&...args) {
    if (level_ >= LogLevel::ERROR) {
      println("ERROR", fmt, std::forward<Args>(args)...);
    }
  }

private:
  LogLevel level_;

  template <class... Args>
  void println(std::string_view level, std::format_string<Args...> fmt,
               Args &&...args) {
    auto now = std::chrono::floor<std::chrono::seconds>(
        std::chrono::system_clock::now());

    auto str =
        std::format("[{:%H:%M:%S}] [{}] [{}] {}\n", now, PROGRAM_NAME, level,
                    std::format(fmt, std::forward<Args>(args)...));

    std::clog << str;
  }
};
} // namespace App
