#include "graphene/platform.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

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

} // namespace graphene::platform
#endif
