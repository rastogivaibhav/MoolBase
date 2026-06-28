#pragma once
#include "graphene/types.hpp"
#include <filesystem>
#include <string>

namespace graphene::platform {

using FileHandle = intptr_t;
static constexpr FileHandle kInvalidFile = -1;

Status open_append(const std::filesystem::path& path, FileHandle* out);
Status write_all(FileHandle handle, const char* data, size_t size);
Status flush(FileHandle handle);
void close_file(FileHandle handle);

uint64_t current_pid();
bool process_is_alive(uint64_t pid);

} // namespace graphene::platform
