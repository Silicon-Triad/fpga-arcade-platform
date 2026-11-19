// Unit tests of the SF-04 power-cut test helpers (#45).
#include "tools/save_loop/save_loop.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace arcade::save_loop {
namespace {

constexpr const char* kPath = "/hs.csv";

/// In-memory file system: rename() moves the synced data, like the real one.
class MapFileSystem final : public FileSystem {
  public:
    std::optional<std::string> readAll(const std::string& path) noexcept override {
        const auto it = files.find(path);
        if (it == files.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    int openForWrite(const std::string& path) noexcept override {
        openPath_ = path;
        files[path].clear();
        return kFd;
    }
    bool writeAll(int fd, std::string_view data) noexcept override {
        files[openPath_].append(data);
        return fd == kFd;
    }
    bool sync(int fd) noexcept override { return fd == kFd; }
    bool close(int fd) noexcept override { return fd == kFd; }
    bool rename(const std::string& from, const std::string& to) noexcept override {
        const auto it = files.find(from);
        if (it == files.end()) {
            return false;
        }
        files[to] = it->second;
        files.erase(it);
        return true;
    }

    static constexpr int kFd = 3;
    std::map<std::string, std::string> files;

  private:
    std::string openPath_;
};

std::string savedText(Pattern pattern) {
    MapFileSystem fs;
    HighScoreStore store = makeStore(pattern);
    EXPECT_EQ(store.save(fs, kPath), SaveStatus::Ok);
    return fs.files[kPath];
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        out.push_back(text.substr(start, end - start));
        start = end == std::string::npos ? text.size() : end + 1;
    }
    return out;
}

TEST(SaveLoop, SF_04_ReferenceTablesAreFullAndDifferent) {
    for (const Pattern p : {Pattern::A, Pattern::B}) {
        const HighScoreStore store = makeStore(p);
        for (std::uint8_t game = 0; game < kHighScoreGameCount; ++game) {
            EXPECT_EQ(store.table(game).count, HISCORE_SLOTS);
        }
    }
    EXPECT_NE(savedText(Pattern::A), savedText(Pattern::B));
}

TEST(SaveLoop, SF_04_CompleteTableAPasses) {
    MapFileSystem fs;
    fs.files[kPath] = savedText(Pattern::A);
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_EQ(r.match, Pattern::A);
    EXPECT_TRUE(passed(r));
    EXPECT_EQ(report(r), "table=A game0=10 game1=10 rejected=0 -> PASS");
}

TEST(SaveLoop, SF_04_CompleteTableBPasses) {
    MapFileSystem fs;
    fs.files[kPath] = savedText(Pattern::B);
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_EQ(r.match, Pattern::B);
    EXPECT_TRUE(passed(r));
}

TEST(SaveLoop, SF_04_MissingFileFails) {
    MapFileSystem fs;
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_FALSE(r.fileFound);
    EXPECT_FALSE(passed(r));
    EXPECT_EQ(report(r), "file missing or unreadable -> FAIL");
}

TEST(SaveLoop, SF_04_TruncatedFileFails) {
    const std::string full = savedText(Pattern::A);
    MapFileSystem fs;
    fs.files[kPath] = full.substr(0, full.size() / 2);  // a write cut off half way
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_FALSE(passed(r));
}

TEST(SaveLoop, SF_04_EmptyFileFails) {
    MapFileSystem fs;
    fs.files[kPath] = "";
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_TRUE(r.fileFound);
    EXPECT_FALSE(r.match.has_value());
    EXPECT_FALSE(passed(r));
}

TEST(SaveLoop, SF_04_MixOfOldAndNewFails) {
    // Lines of table A for game 0 and of table B for game 1: neither complete table.
    std::string mixed;
    for (const auto& line : lines(savedText(Pattern::A))) {
        if (line.rfind("0,", 0) == 0) {
            mixed += line + "\n";
        }
    }
    for (const auto& line : lines(savedText(Pattern::B))) {
        if (line.rfind("1,", 0) == 0) {
            mixed += line + "\n";
        }
    }
    MapFileSystem fs;
    fs.files[kPath] = mixed;
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_EQ(r.entries[0], HISCORE_SLOTS);
    EXPECT_EQ(r.entries[1], HISCORE_SLOTS);
    EXPECT_FALSE(r.match.has_value());
    EXPECT_FALSE(passed(r));
}

TEST(SaveLoop, SC_04_RejectedLineFails) {
    MapFileSystem fs;
    fs.files[kPath] = savedText(Pattern::A) + "garbage\n";
    const CheckResult r = checkFile(fs, kPath);
    EXPECT_EQ(r.rejectedLines, 1U);
    EXPECT_EQ(r.match, Pattern::A);  // load() drops the line, the table is still A
    EXPECT_FALSE(passed(r));
}

TEST(SaveLoop, SR_FW_09_RealFileSystemAlternatesAndChecks) {
    const std::string path = ::testing::TempDir() + "save_loop_test.csv";
    PosixFileSystem fs;
    HighScoreStore a = makeStore(Pattern::A);
    HighScoreStore b = makeStore(Pattern::B);
    ASSERT_EQ(a.save(fs, path), SaveStatus::Ok);
    ASSERT_EQ(b.save(fs, path), SaveStatus::Ok);
    const CheckResult r = checkFile(fs, path);
    EXPECT_EQ(r.match, Pattern::B);
    EXPECT_TRUE(passed(r));
    (void)std::remove(path.c_str());
}

bool parse(std::vector<const char*> args, Options& out, std::string& error) {
    args.insert(args.begin(), "save_loop");
    return parseOptions(static_cast<int>(args.size()), args.data(), out, error);
}

TEST(SaveLoopOptions, Defaults) {
    Options o;
    std::string e;
    ASSERT_TRUE(parse({"check"}, o, e));
    EXPECT_EQ(o.mode, Mode::Check);
    EXPECT_EQ(o.file, std::string(HISCORE_FILE));
    EXPECT_EQ(o.count, 0U);
}

TEST(SaveLoopOptions, WriteWithFileAndCount) {
    Options o;
    std::string e;
    ASSERT_TRUE(parse({"write", "--file", "/tmp/x.csv", "--count", "42"}, o, e));
    EXPECT_EQ(o.mode, Mode::Write);
    EXPECT_EQ(o.file, "/tmp/x.csv");
    EXPECT_EQ(o.count, 42U);
}

TEST(SaveLoopOptions, HelpNeedsNoMode) {
    Options o;
    std::string e;
    ASSERT_TRUE(parse({"--help"}, o, e));
    EXPECT_TRUE(o.help);
}

TEST(SaveLoopOptions, RejectsBadInput) {
    const std::vector<std::vector<const char*>> bad = {
        {},
        {"write", "check"},
        {"write", "--count"},
        {"write", "--count", "-1"},
        {"write", "--count", "12x"},
        {"write", "--file", ""},
        {"run"},
    };
    for (const auto& args : bad) {
        Options o;
        std::string e;
        EXPECT_FALSE(parse(args, o, e));
        EXPECT_FALSE(e.empty());
    }
}

}  // namespace
}  // namespace arcade::save_loop
