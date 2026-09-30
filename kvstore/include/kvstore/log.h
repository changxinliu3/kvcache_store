#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace kvstore {

enum class LogLevel {
  TRACE = 0,
  DEBUG,
  INFO,
  WARN,
  ERROR,
  OFF,
};

inline std::atomic<LogLevel>& global_log_level() {
  static std::atomic<LogLevel> level{LogLevel::INFO};
  return level;
}

inline void set_log_level(LogLevel level) { global_log_level().store(level); }

inline void kv_log(LogLevel level, const char* tag, const char* fmt, ...) {
  if (level < global_log_level().load()) {
    return;
  }
  const char* name = "INFO";
  switch (level) {
    case LogLevel::TRACE:
      name = "TRACE";
      break;
    case LogLevel::DEBUG:
      name = "DEBUG";
      break;
    case LogLevel::INFO:
      name = "INFO";
      break;
    case LogLevel::WARN:
      name = "WARN";
      break;
    case LogLevel::ERROR:
      name = "ERROR";
      break;
    case LogLevel::OFF:
      return;
  }
  std::fprintf(stderr, "[%s][%s] ", name, tag);
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);
  std::fprintf(stderr, "\n");
}

#define KV_LOG_TRACE(tag, ...) ::kvstore::kv_log(::kvstore::LogLevel::TRACE, tag, __VA_ARGS__)
#define KV_LOG_DEBUG(tag, ...) ::kvstore::kv_log(::kvstore::LogLevel::DEBUG, tag, __VA_ARGS__)
#define KV_LOG_INFO(tag, ...) ::kvstore::kv_log(::kvstore::LogLevel::INFO, tag, __VA_ARGS__)
#define KV_LOG_WARN(tag, ...) ::kvstore::kv_log(::kvstore::LogLevel::WARN, tag, __VA_ARGS__)
#define KV_LOG_ERROR(tag, ...) ::kvstore::kv_log(::kvstore::LogLevel::ERROR, tag, __VA_ARGS__)

}  // namespace kvstore
