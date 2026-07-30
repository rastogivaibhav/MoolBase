#include "graphene/platform.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <windows.h>

namespace graphene::platform {

Status open_append(const std::filesystem::path& path, FileHandle* out) {
  if (!out) return Status::error(ErrorCode::InvalidInput, "out file handle cannot be null");
  HANDLE h = ::CreateFileW(path.wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return Status::error(ErrorCode::IoError, "CreateFileW append failed");
  *out = reinterpret_cast<FileHandle>(h);
  return Status::ok();
}

Status write_all(FileHandle handle, const char* data, size_t size) {
  HANDLE h = reinterpret_cast<HANDLE>(handle);
  size_t remaining = size;
  while (remaining > 0) {
    DWORD chunk = remaining > 0x7ffff000u ? 0x7ffff000u : static_cast<DWORD>(remaining);
    DWORD written = 0;
    if (!::WriteFile(h, data, chunk, &written, nullptr)) return Status::error(ErrorCode::IoError, "WriteFile failed");
    data += written;
    remaining -= written;
  }
  return Status::ok();
}

Status flush(FileHandle handle) {
  HANDLE h = reinterpret_cast<HANDLE>(handle);
  if (!::FlushFileBuffers(h)) return Status::error(ErrorCode::IoError, "FlushFileBuffers failed");
  return Status::ok();
}

void close_file(FileHandle handle) {
  if (handle != kInvalidFile) ::CloseHandle(reinterpret_cast<HANDLE>(handle));
}

Status create_file_exclusive(const std::filesystem::path& path, const std::string& contents) {
  HANDLE h = ::CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (::GetLastError() == ERROR_FILE_EXISTS || ::GetLastError() == ERROR_ALREADY_EXISTS) {
      return Status::error(ErrorCode::LockBusy, "file already exists: " + path.string());
    }
    return Status::error(ErrorCode::IoError, "CreateFileW exclusive create failed");
  }
  auto st = write_all(reinterpret_cast<FileHandle>(h), contents.data(), contents.size());
  if (st) st = flush(reinterpret_cast<FileHandle>(h));
  const BOOL close_result = ::CloseHandle(h);
  if (st && !close_result) st = Status::error(ErrorCode::IoError, "CloseHandle exclusive file failed");
  if (!st) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
  return st;
}

Status flush_path(const std::filesystem::path& path) {
  HANDLE h = ::CreateFileW(path.wstring().c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return Status::error(ErrorCode::IoError, "CreateFileW flush path failed");
  const BOOL flushed = ::FlushFileBuffers(h);
  const BOOL closed = ::CloseHandle(h);
  if (!flushed) return Status::error(ErrorCode::IoError, "FlushFileBuffers path failed");
  if (!closed) return Status::error(ErrorCode::IoError, "CloseHandle flushed path failed");
  return Status::ok();
}

Status durable_replace(const std::filesystem::path& source, const std::filesystem::path& destination) {
  auto st = flush_path(source);
  if (!st) return st;
  DWORD last_error = ERROR_SUCCESS;
  for (int attempt = 0; attempt < 10; ++attempt) {
    if (::MoveFileExW(source.wstring().c_str(), destination.wstring().c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      return Status::ok();
    }
    last_error = ::GetLastError();
    if (last_error != ERROR_SHARING_VIOLATION &&
        last_error != ERROR_ACCESS_DENIED &&
        last_error != ERROR_LOCK_VIOLATION) {
      break;
    }
    // Virus scanners and short-lived readers can transiently retain a Windows
    // file handle after close. Bounded retry preserves atomic replacement
    // semantics without falling back to delete-then-rename.
    ::Sleep(static_cast<DWORD>(attempt + 1));
  }
  return Status::error(
    ErrorCode::IoError,
    "MoveFileExW atomic replace failed with Windows error " + std::to_string(last_error));
}

Status truncate_and_flush(const std::filesystem::path& path) {
  HANDLE h = ::CreateFileW(path.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return Status::error(ErrorCode::IoError, "CreateFileW truncate failed");
  const BOOL flushed = ::FlushFileBuffers(h);
  const BOOL closed = ::CloseHandle(h);
  if (!flushed) return Status::error(ErrorCode::IoError, "FlushFileBuffers truncated file failed");
  if (!closed) return Status::error(ErrorCode::IoError, "CloseHandle truncated file failed");
  return Status::ok();
}

uint64_t current_pid() { return static_cast<uint64_t>(::GetCurrentProcessId()); }

bool process_is_alive(uint64_t pid) {
  if (pid <= 1) return true;
  HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (!h) return false;
  DWORD code = 0;
  bool alive = (::GetExitCodeProcess(h, &code) && code == STILL_ACTIVE);
  ::CloseHandle(h);
  return alive;
}

std::optional<std::chrono::system_clock::time_point> current_process_start_time() {
  FILETIME create_time{}, exit_time{}, kernel_time{}, user_time{};
  if (!::GetProcessTimes(::GetCurrentProcess(), &create_time, &exit_time, &kernel_time, &user_time)) {
    return std::nullopt;
  }
  ULARGE_INTEGER ticks{};
  ticks.LowPart = create_time.dwLowDateTime;
  ticks.HighPart = create_time.dwHighDateTime;
  constexpr uint64_t kUnixEpochDiff100ns = 116444736000000000ULL;
  if (ticks.QuadPart < kUnixEpochDiff100ns) return std::nullopt;
  const uint64_t unix_100ns = ticks.QuadPart - kUnixEpochDiff100ns;
  return std::chrono::system_clock::time_point{
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds(unix_100ns * 100ULL))};
}

} // namespace graphene::platform
#endif
