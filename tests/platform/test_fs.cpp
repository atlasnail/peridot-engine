#include "framework.hpp"

#include <filesystem>
#include <string>
#include <system_error>

#include "peri/platform/fs.hpp"

namespace {

std::filesystem::path test_directory() {
    // CTest runs from the build directory; keep test artifacts inside the project.
    return std::filesystem::current_path() / "peri_fs_test";
}

void reset_test_directory() {
    std::error_code error;
    std::filesystem::remove_all(test_directory(), error);
    std::filesystem::create_directories(test_directory(), error);
}

void cleanup_test_directory() {
    std::error_code error;
    std::filesystem::remove_all(test_directory(), error);
}

}  // namespace

TEST_CASE(fs_write_read_roundtrip) {
    reset_test_directory();
    const std::string directory = test_directory().string();
    const std::string path = peri::fs::join(directory, "hello.txt");

    REQUIRE(peri::fs::write_text_file(path, "hello\nworld").is_ok());

    const peri::Result<std::string> read = peri::fs::read_text_file(path);
    REQUIRE(read.is_ok());
    CHECK(read.value() == "hello\nworld");

    cleanup_test_directory();
}

TEST_CASE(fs_atomic_write_replaces_and_leaves_no_temp) {
    reset_test_directory();
    const std::string directory = test_directory().string();
    const std::string path = peri::fs::join(directory, "save.json");

    REQUIRE(peri::fs::write_text_file(path, "old").is_ok());
    REQUIRE(peri::fs::write_text_file_atomic(path, "new").is_ok());

    const peri::Result<std::string> read = peri::fs::read_text_file(path);
    REQUIRE(read.is_ok());
    CHECK(read.value() == "new");
    CHECK(!peri::fs::exists(path + ".tmp"));

    cleanup_test_directory();
}

TEST_CASE(fs_rejects_embedded_nul_paths) {
    reset_test_directory();
    const std::string target = peri::fs::join(test_directory().string(), "target.txt");
    REQUIRE(peri::fs::write_text_file(target, "original").is_ok());
    std::string invalid_path = target;
    invalid_path.push_back('\0');
    invalid_path += "suffix";

    const auto read = peri::fs::read_text_file(invalid_path);
    REQUIRE(read.is_error());
    CHECK(read.error().code == peri::ErrorCode::invalid_argument);
    const auto plain = peri::fs::write_text_file(invalid_path, "wrong");
    REQUIRE(plain.is_error());
    CHECK(plain.error().code == peri::ErrorCode::invalid_argument);
    const auto atomic = peri::fs::write_text_file_atomic(invalid_path, "wrong");
    REQUIRE(atomic.is_error());
    CHECK(atomic.error().code == peri::ErrorCode::invalid_argument);
    CHECK(!peri::fs::exists(invalid_path));
    const auto directory = peri::fs::create_directories(invalid_path);
    REQUIRE(directory.is_error());
    CHECK(directory.error().code == peri::ErrorCode::invalid_argument);
    const auto original = peri::fs::read_text_file(target);
    REQUIRE(original.is_ok());
    CHECK(original.value() == "original");
    CHECK(!peri::fs::exists(target + ".tmp"));
    cleanup_test_directory();
}

TEST_CASE(fs_missing_file_is_not_found) {
    reset_test_directory();
    const std::string path = peri::fs::join(test_directory().string(), "absent.txt");

    const peri::Result<std::string> read = peri::fs::read_text_file(path);
    CHECK(read.is_error());
    CHECK(read.error().code == peri::ErrorCode::not_found);

    cleanup_test_directory();
}

TEST_CASE(fs_creates_directories_and_handles_utf8_names) {
    reset_test_directory();
    const std::string nested = peri::fs::join(test_directory().string(), "caf\u00e9/sub");
    REQUIRE(peri::fs::create_directories(nested).is_ok());
    CHECK(peri::fs::exists(nested));

    const std::string path = peri::fs::join(nested, "na\u00efve.txt");
    REQUIRE(peri::fs::write_text_file(path, "unicode").is_ok());

    const peri::Result<std::string> read = peri::fs::read_text_file(path);
    REQUIRE(read.is_ok());
    CHECK(read.value() == "unicode");

    cleanup_test_directory();
}

TEST_CASE(fs_join_handles_trailing_separators) {
    CHECK(peri::fs::join("", "file.txt") == "file.txt");
    CHECK(peri::fs::join("dir", "file.txt") == "dir/file.txt");
    CHECK(peri::fs::join("dir/", "file.txt") == "dir/file.txt");
}

TEST_CASE(fs_executable_directory_exists) {
    const peri::Result<std::string> directory = peri::fs::executable_directory();
    REQUIRE(directory.is_ok());
    CHECK(!directory.value().empty());
    CHECK(peri::fs::exists(directory.value()));
}

TEST_CASE(fs_create_directories_rejects_existing_file) {
    reset_test_directory();
    const std::string path = peri::fs::join(test_directory().string(), "file.txt");
    REQUIRE(peri::fs::write_text_file(path, "file").is_ok());
    CHECK(peri::fs::create_directories(path).is_error());
    cleanup_test_directory();
}

TEST_CASE(fs_reading_a_directory_is_an_error) {
    reset_test_directory();
    const peri::Result<std::string> read = peri::fs::read_text_file(test_directory().string());
    REQUIRE(read.is_error());
    CHECK(read.error().code == peri::ErrorCode::io_failure);
    cleanup_test_directory();
}
