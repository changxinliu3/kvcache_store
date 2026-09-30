#pragma once

#include <string_view>

namespace kvstore {

enum class Status {
  OK = 0,
  INVALID_ARGUMENT,
  NOT_FOUND,
  ALREADY_EXISTS,
  NO_SPACE,
  IO_ERROR,
  IO_IN_PROGRESS,
  IO_FAILED,
  INTERNAL_ERROR,
  NOT_SUPPORTED,
};

inline constexpr bool ok(Status s) { return s == Status::OK; }

inline constexpr std::string_view to_string(Status s) {
  switch (s) {
    case Status::OK:
      return "OK";
    case Status::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case Status::NOT_FOUND:
      return "NOT_FOUND";
    case Status::ALREADY_EXISTS:
      return "ALREADY_EXISTS";
    case Status::NO_SPACE:
      return "NO_SPACE";
    case Status::IO_ERROR:
      return "IO_ERROR";
    case Status::IO_IN_PROGRESS:
      return "IO_IN_PROGRESS";
    case Status::IO_FAILED:
      return "IO_FAILED";
    case Status::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case Status::NOT_SUPPORTED:
      return "NOT_SUPPORTED";
  }
  return "UNKNOWN";
}

}  // namespace kvstore
