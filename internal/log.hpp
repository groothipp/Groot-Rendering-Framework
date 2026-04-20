#pragma once

#include <format>
#include <print>
#include <source_location>

namespace grf {
namespace log {

template <typename... Args>
void generic(std::format_string<Args...> fmt, Args... args) {
  std::println(stderr, "[GRF] {}", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void success(std::format_string<Args...> fmt, Args... args) {
  std::println(stderr, "\033[32m[GRF] {}\033[0m", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warning(std::format_string<Args...> fmt, Args... args) {
  std::println(stderr, "\033[33m[GRF] {}\033[0m", std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args... args) {
  std::println(stderr, "\033[31m[GRF] {}\033[0m", std::format(fmt, std::forward<Args>(args)...));
}

}

template <typename... Args>
[[noreturn]] void panic_impl(
  std::source_location loc,
  std::format_string<Args...> fmt,
  Args... args
) {
  log::error("PANIC ({}:{}) - {}", loc.file_name(), loc.line(), std::format(fmt, std::forward<Args>(args)...));
  std::abort();
}

}

#define GRF_PANIC(...) ::grf::panic_impl(std::source_location::current(), __VA_ARGS__)