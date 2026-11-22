#pragma once

// File system helpers.
//
// Paths are UTF-8 strings and use '/' as the separator everywhere; Windows
// accepts '/' as well. Conversion to native wide paths happens in the
// implementation. Paths containing embedded NUL bytes are rejected (exists()
// returns false). Writes are explicit: a plain write can leave a partial file,
// while write_text_file_atomic() replaces the destination only after the new
// contents are complete. A stream flush is not a durable fsync; power-loss
// protection and directory syncing are not implemented.
//
// Atomic replace currently assumes one writer per path. The save system (M6)
// adds locking and backups on top of these primitives.

#include <string>
#include <string_view>

#include "peri/core/result.hpp"

namespace peri::fs {

// Reads a whole text file. Returns ErrorCode::not_found when it does not exist,
// io_failure when it cannot be read.
[[nodiscard]] Result<std::string> read_text_file(std::string_view path);

// Writes a whole file, truncating an existing one. Not crash-safe.
Status write_text_file(std::string_view path, std::string_view contents);

// Writes to a temporary file in the same directory, flushes it, then replaces
// the destination. On failure the destination is left untouched and the
// removal of the temporary file is attempted.
Status write_text_file_atomic(std::string_view path, std::string_view contents);

[[nodiscard]] bool exists(std::string_view path);

Status create_directories(std::string_view path);

// Joins with a single '/', without normalizing '..' or duplicate separators.
[[nodiscard]] std::string join(std::string_view directory, std::string_view name);

// Directory containing the running executable, with a trailing separator.
[[nodiscard]] Result<std::string> executable_directory();

}  // namespace peri::fs
