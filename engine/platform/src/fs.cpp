#include "peri/platform/fs.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

#include <SDL3/SDL.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "peri/core/log.hpp"

namespace peri::fs {
namespace {

bool valid_path(std::string_view path) noexcept {
    return path.find('\0') == std::string_view::npos;
}

#if defined(_WIN32)
std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    // Copy to a null-terminated string: the conversion API takes a C string and
    // clang-tidy cannot see that the size is passed alongside the view.
    const std::string text(utf8);
    const int size = static_cast<int>(text.size());
    const int needed =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), size, nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<usize>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), size, wide.data(), needed);
    return wide;
}

std::filesystem::path to_path(std::string_view utf8) {
    return std::filesystem::path(widen(utf8));
}
#else
std::filesystem::path to_path(std::string_view utf8) {
    return std::filesystem::path(std::string(utf8));
}
#endif

bool replace_file(const std::string& from, const std::string& to) {
#if defined(_WIN32)
    const std::filesystem::path source = to_path(from);
    const std::filesystem::path destination = to_path(to);
    if (!MoveFileExW(source.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        PERI_LOG_ERROR("fs", "atomic replace failed for %s", to.c_str());
        return false;
    }
    return true;
#else
    std::error_code error;
    std::filesystem::rename(to_path(from), to_path(to), error);
    if (error) {
        PERI_LOG_ERROR("fs", "atomic replace failed for %s: %s", to.c_str(),
                       error.message().c_str());
        return false;
    }
    return true;
#endif
}

}  // namespace

Result<std::string> read_text_file(std::string_view path) {
    if (!valid_path(path)) {
        PERI_LOG_ERROR("fs", "file path contains an embedded NUL");
        return Error{ErrorCode::invalid_argument, "file path contains an embedded NUL"};
    }
    const std::filesystem::path fs_path = to_path(path);

    std::error_code error;
    const bool file_exists = std::filesystem::exists(fs_path, error);
    if (!file_exists || error) {
        return Error{ErrorCode::not_found, "file not found"};
    }
    if (std::filesystem::is_directory(fs_path, error) && !error) {
        return Error{ErrorCode::io_failure, "path is a directory"};
    }

    // Read by size rather than with istreambuf_iterator: GCC 13 at -O3 reports
    // a null-dereference false positive inside libstdc++'s streambuf when the
    // iterator form is inlined, which fails this project's warnings-as-errors
    // build. A sized read also avoids repeated string growth.
    std::ifstream file(fs_path, std::ios::binary | std::ios::ate);
    if (!file) {
        PERI_LOG_ERROR("fs", "could not open %s for reading", std::string(path).c_str());
        return Error{ErrorCode::io_failure, "could not open file for reading"};
    }

    const std::streamoff end = file.tellg();
    if (end < 0) {
        PERI_LOG_ERROR("fs", "could not size %s", std::string(path).c_str());
        return Error{ErrorCode::io_failure, "could not determine file size"};
    }

    std::string contents(static_cast<std::size_t>(end), '\0');
    file.seekg(0, std::ios::beg);
    if (!contents.empty()) {
        file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!file) {
            PERI_LOG_ERROR("fs", "short read of %s", std::string(path).c_str());
            return Error{ErrorCode::io_failure, "error while reading file"};
        }
    }

    return contents;
}

Status write_text_file(std::string_view path, std::string_view contents) {
    if (!valid_path(path)) {
        PERI_LOG_ERROR("fs", "file path contains an embedded NUL");
        return Status::failure(ErrorCode::invalid_argument, "file path contains an embedded NUL");
    }
    const std::filesystem::path fs_path = to_path(path);

    std::ofstream file(fs_path, std::ios::binary | std::ios::trunc);
    if (!file) {
        PERI_LOG_ERROR("fs", "could not open %s for writing", std::string(path).c_str());
        return Status::failure(ErrorCode::io_failure, "could not open file for writing");
    }

    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.flush();
    if (!file) {
        PERI_LOG_ERROR("fs", "error while writing %s", std::string(path).c_str());
        return Status::failure(ErrorCode::io_failure, "error while writing file");
    }

    return Status::ok();
}

Status write_text_file_atomic(std::string_view path, std::string_view contents) {
    if (!valid_path(path)) {
        PERI_LOG_ERROR("fs", "file path contains an embedded NUL");
        return Status::failure(ErrorCode::invalid_argument, "file path contains an embedded NUL");
    }
    std::string temp(path);
    temp += ".tmp";

    const Status written = write_text_file(temp, contents);
    if (written.is_error()) {
        std::error_code ignored;
        std::filesystem::remove(to_path(temp), ignored);
        return written;
    }

    if (!replace_file(temp, std::string(path))) {
        std::error_code ignored;
        std::filesystem::remove(to_path(temp), ignored);
        return Status::failure(ErrorCode::io_failure, "atomic replace failed");
    }

    return Status::ok();
}

bool exists(std::string_view path) {
    if (!valid_path(path)) {
        return false;
    }
    std::error_code error;
    const bool result = std::filesystem::exists(to_path(path), error);
    return !error && result;
}

Status create_directories(std::string_view path) {
    if (!valid_path(path)) {
        PERI_LOG_ERROR("fs", "directory path contains an embedded NUL");
        return Status::failure(ErrorCode::invalid_argument,
                               "directory path contains an embedded NUL");
    }
    const std::filesystem::path fs_path = to_path(path);

    std::error_code error;
    if (std::filesystem::is_directory(fs_path, error) && !error) {
        return Status::ok();
    }
    error.clear();
    std::filesystem::create_directories(fs_path, error);
    if (error) {
        PERI_LOG_ERROR("fs", "could not create directory %s: %s", std::string(path).c_str(),
                       error.message().c_str());
        return Status::failure(ErrorCode::io_failure, "could not create directory");
    }

    return Status::ok();
}

std::string join(std::string_view directory, std::string_view name) {
    if (directory.empty()) {
        return std::string(name);
    }

    std::string result(directory);
    const char last = result.back();
    if (last != '/' && last != '\\') {
        result.push_back('/');
    }
    result.append(name);
    return result;
}

Result<std::string> executable_directory() {
    // SDL3 returns a pointer to internal storage; do not free it.
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        PERI_LOG_ERROR("fs", "SDL_GetBasePath failed: %s", SDL_GetError());
        return Error{ErrorCode::io_failure, "could not determine executable directory"};
    }

    return std::string(base);
}

}  // namespace peri::fs
