#include "graphene/platform.hpp"

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

namespace graphene::platform {

Status open_append(const std::filesystem::path& path, FileHandle* out) {
  if (!out) return Status::error(ErrorCode::InvalidInput, "out file handle cannot be null");
  int fd = ::open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
  if (fd < 0) return Status::error(ErrorCode::IoError, std::string("open append failed: ") + std::strerror(errno));
  *out = static_cast<FileHandle>(fd);
  return Status::ok();
}

Status write_all(FileHandle handle, const char* data, size_t size) {
  int fd = static_cast<int>(handle);
  size_t remaining = size;
  while (remaining > 0) {
    ssize_t n = ::write(fd, data, remaining);
    if (n < 0) return Status::error(ErrorCode::IoError, std::string("write failed: ") + std::strerror(errno));
    data += n;
    remaining -= static_cast<size_t>(n);
  }
  return Status::ok();
}

Status flush(FileHandle handle) {
  int fd = static_cast<int>(handle);
  if (::fsync(fd) != 0) return Status::error(ErrorCode::IoError, std::string("fsync failed: ") + std::strerror(errno));
  return Status::ok();
}

void close_file(FileHandle handle) {
  if (handle != kInvalidFile) ::close(static_cast<int>(handle));
}

uint64_t current_pid() { return static_cast<uint64_t>(::getpid()); }

bool process_is_alive(uint64_t pid) {
  if (pid <= 1) return true;
  return (::kill(static_cast<pid_t>(pid), 0) == 0 || errno != ESRCH);
}

} // namespace graphene::platform
#endif
