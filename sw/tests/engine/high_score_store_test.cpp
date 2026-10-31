// Unit tests for the high-score store (issue #11, SR-FW-08, SR-FW-09, SR-FW-10, SC-03, SC-04).
#include "engine/high_score_store.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace arcade {
namespace {

constexpr const char* kPath = "/hs.csv";
constexpr const char* kTempPath = "/hs.csv.tmp";

/// Fake file system: files live in a map; every call is logged so tests can check the order.
class FakeFileSystem final : public FileSystem {
  public:
    std::optional<std::string> readAll(const std::string& path) noexcept override {
        log.emplace_back("read " + path);
        const auto it = files.find(path);
        if (it == files.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    int openForWrite(const std::string& path) noexcept override {
        log.emplace_back("open " + path);
        if (failOpen) {
            return -1;
        }
        openPath = path;
        pending.clear();
        return kFd;
    }
    bool writeAll(int fd, std::string_view data) noexcept override {
        log.emplace_back("write");
        if (fd != kFd || failWrite) {
            return false;
        }
        pending.append(data);
        return true;
    }
    bool sync(int fd) noexcept override {
        log.emplace_back("fsync");
        if (fd != kFd || failSync) {
            return false;
        }
        files[openPath] = pending;  // Data reaches the card only on fsync().
        return true;
    }
    bool close(int fd) noexcept override {
        log.emplace_back("close");
        return fd == kFd;
    }
    bool rename(const std::string& from, const std::string& to) noexcept override {
        log.emplace_back("rename " + from + " " + to);
        const auto it = files.find(from);
        if (it == files.end()) {
            return false;
        }
        files[to] = it->second;
        files.erase(it);
        return true;
    }

    static constexpr int kFd = 7;
    std::map<std::string, std::string> files;
    std::vector<std::string> log;
    std::string openPath;
    std::string pending;
    bool failOpen = false;
    bool failWrite = false;
    bool failSync = false;
};

std::array<char, INITIALS_LEN> ini(std::string_view s) { return {s[0], s[1], s[2]}; }

std::string initialsOf(const HighScoreRow& r) { return {r.initials.data(), INITIALS_LEN}; }

HighScoreStore loadFrom(FakeFileSystem& fs, const std::string& text) {
    fs.files[kPath] = text;
    HighScoreStore store;
    store.load(fs, kPath);
    return store;
}

TEST(HighScoreStore, SR_FW_08_MissingFileGivesEmptyTable) {
    FakeFileSystem fs;
    HighScoreStore store;
    store.load(fs, kPath);
    EXPECT_EQ(store.table(kGameIdSnake).count, 0u);
    EXPECT_EQ(store.table(kGameIdBreakout).count, 0u);
}

TEST(HighScoreStore, SR_FW_08_EmptyFileGivesEmptyTable) {
    FakeFileSystem fs;
    const HighScoreStore store = loadFrom(fs, "");
    EXPECT_EQ(store.table(kGameIdSnake).count, 0u);
    EXPECT_EQ(store.table(kGameIdBreakout).count, 0u);
}

TEST(HighScoreStore, SR_FW_08_LoadsValidFile) {
    FakeFileSystem fs;
    const HighScoreStore store = loadFrom(fs, "0,1,ABC,500,3\n0,2,XYZ,100,1\n1,1,QQQ,999999,2\n");
    const auto& snake = store.table(kGameIdSnake);
    ASSERT_EQ(snake.count, 2u);
    EXPECT_EQ(initialsOf(snake.rows[0]), "ABC");
    EXPECT_EQ(snake.rows[0].score, 500u);
    EXPECT_EQ(snake.rows[0].rank, 1);
    EXPECT_EQ(snake.rows[1].rank, 2);
    ASSERT_EQ(store.table(kGameIdBreakout).count, 1u);
    EXPECT_EQ(store.table(kGameIdBreakout).rows[0].score, SCORE_MAX);
}

TEST(HighScoreStore, SC_04_CorruptedFileDropsBadLinesKeepsGood) {
    FakeFileSystem fs;
    std::string text =
        "0,1,ABC,500,1\n"
        "0,2,abc,400,2\n"            // lower-case initials
        "0,2,AB,400,3\n"             // two letters
        "0,2,ABCD,400,4\n"           // four letters
        "0,2,A1C,400,5\n"            // digit in initials
        "0,2,DEF,1000000,6\n"        // score above SCORE_MAX
        "0,2,DEF,-5,7\n"             // negative score
        "0,0,DEF,400,8\n"            // rank 0
        "0,11,DEF,400,9\n"           // rank 11
        "9,2,DEF,400,10\n"           // unknown game
        "0,2,DEF,400\n"              // too few columns
        "0,2,DEF,400,11,x\n"         // too many columns
        "0,2,DEF,4x0,12\n"           // non-digit score
        "0,2,DEF,400,99999999999\n"  // seq overflows UINT32
        "\n"
        "garbage\n";
    text += std::string("\x00\xff\x01,\xfe", 5) + "\n";  // binary junk
    text += "1,1,GHI,300,13\r\n";                        // CRLF line end is accepted
    const HighScoreStore store = loadFrom(fs, text);
    ASSERT_EQ(store.table(kGameIdSnake).count, 1u);
    EXPECT_EQ(initialsOf(store.table(kGameIdSnake).rows[0]), "ABC");
    ASSERT_EQ(store.table(kGameIdBreakout).count, 1u);
    EXPECT_EQ(initialsOf(store.table(kGameIdBreakout).rows[0]), "GHI");
}

TEST(HighScoreStore, SC_04_ParseLineAcceptsBoundaries) {
    EXPECT_TRUE(HighScoreStore::parseLine("0,1,AAA,0,0"));
    EXPECT_TRUE(HighScoreStore::parseLine("1,10,ZZZ,999999,4294967295"));
    EXPECT_FALSE(HighScoreStore::parseLine(""));
    EXPECT_FALSE(HighScoreStore::parseLine("0,1,AAA,,0"));
    EXPECT_FALSE(HighScoreStore::parseLine("0,1,AAA, 5,0"));
}

TEST(HighScoreStore, SR_FW_10_LoadSortsAndReranksByScoreThenSeq) {
    FakeFileSystem fs;
    // File ranks are wrong on purpose; the store ranks by score, then older seq first.
    const HighScoreStore store = loadFrom(fs, "0,1,LOW,10,1\n0,1,NEW,50,9\n0,1,OLD,50,2\n");
    const auto& t = store.table(kGameIdSnake);
    ASSERT_EQ(t.count, 3u);
    EXPECT_EQ(initialsOf(t.rows[0]), "OLD");
    EXPECT_EQ(initialsOf(t.rows[1]), "NEW");
    EXPECT_EQ(initialsOf(t.rows[2]), "LOW");
    EXPECT_EQ(t.rows[2].rank, 3);
}

TEST(HighScoreStore, SR_FW_10_TiesRankOlderEntryFirst) {
    HighScoreStore store;
    EXPECT_EQ(store.insert(kGameIdSnake, ini("AAA"), 100), std::optional<std::uint8_t>(1));
    EXPECT_EQ(store.insert(kGameIdSnake, ini("BBB"), 100), std::optional<std::uint8_t>(2));
    EXPECT_EQ(store.insert(kGameIdSnake, ini("CCC"), 200), std::optional<std::uint8_t>(1));
    const auto& t = store.table(kGameIdSnake);
    EXPECT_EQ(initialsOf(t.rows[0]), "CCC");
    EXPECT_EQ(initialsOf(t.rows[1]), "AAA");
    EXPECT_EQ(initialsOf(t.rows[2]), "BBB");
}

TEST(HighScoreStore, SR_FW_10_EleventhEntryDropsLowest) {
    HighScoreStore store;
    for (std::uint32_t i = 1; i <= HISCORE_SLOTS; ++i) {
        ASSERT_TRUE(store.insert(kGameIdBreakout, ini("AAA"), i * 10));
    }
    EXPECT_EQ(store.table(kGameIdBreakout).count, HISCORE_SLOTS);
    EXPECT_FALSE(store.qualifies(kGameIdBreakout, 10));  // Equal to lowest: ranks below it.
    EXPECT_FALSE(store.insert(kGameIdBreakout, ini("NOT"), 5));
    EXPECT_EQ(store.insert(kGameIdBreakout, ini("NEW"), 55), std::optional<std::uint8_t>(6));
    const auto& t = store.table(kGameIdBreakout);
    ASSERT_EQ(t.count, HISCORE_SLOTS);
    EXPECT_EQ(t.rows[HISCORE_SLOTS - 1].score, 20u);  // 10 was dropped.
    EXPECT_EQ(t.rows[HISCORE_SLOTS - 1].rank, kRankMax);
    EXPECT_EQ(store.table(kGameIdSnake).count, 0u);  // Tables are per game.
}

TEST(HighScoreStore, SR_FW_10_LoadKeepsOnlyTopTen) {
    FakeFileSystem fs;
    std::string text;
    for (int i = 0; i < 12; ++i) {
        text += "0,1,AAA," + std::to_string(i) + "," + std::to_string(i) + "\n";
    }
    const HighScoreStore store = loadFrom(fs, text);
    const auto& t = store.table(kGameIdSnake);
    ASSERT_EQ(t.count, HISCORE_SLOTS);
    EXPECT_EQ(t.rows[0].score, 11u);
    EXPECT_EQ(t.rows[HISCORE_SLOTS - 1].score, 2u);
}

TEST(HighScoreStore, SR_FW_10_NewEntryGetsSeqAfterLoadedSeq) {
    FakeFileSystem fs;
    HighScoreStore store = loadFrom(fs, "0,1,OLD,100,41\n");
    EXPECT_EQ(store.insert(kGameIdSnake, ini("NEW"), 100), std::optional<std::uint8_t>(2));
    EXPECT_EQ(store.table(kGameIdSnake).rows[1].seq, 42u);
}

TEST(HighScoreStore, SR_FW_10_LoadedMaxSeqBlocksFurtherInserts) {
    FakeFileSystem fs;
    HighScoreStore store = loadFrom(fs, "0,1,OLD,100,4294967295\n");
    EXPECT_FALSE(store.insert(kGameIdSnake, ini("NEW"), 200));
    ASSERT_EQ(store.table(kGameIdSnake).count, 1u);
    EXPECT_EQ(initialsOf(store.table(kGameIdSnake).rows[0]), "OLD");
}

TEST(HighScoreStore, SR_FW_10_LastSeqIsUsedOnceThenInsertsStop) {
    FakeFileSystem fs;
    HighScoreStore store = loadFrom(fs, "0,1,OLD,100,4294967294\n");
    EXPECT_EQ(store.insert(kGameIdSnake, ini("NEW"), 100), std::optional<std::uint8_t>(2));
    EXPECT_EQ(store.table(kGameIdSnake).rows[1].seq, 4294967295u);
    EXPECT_FALSE(store.insert(kGameIdSnake, ini("DUP"), 100));
    EXPECT_EQ(store.table(kGameIdSnake).count, 2u);
}

TEST(HighScoreStore, SC_04_InsertRejectsInvalidEntries) {
    HighScoreStore store;
    EXPECT_FALSE(store.insert(kGameIdSnake, ini("ab1"), 10));
    EXPECT_FALSE(store.insert(kGameIdSnake, ini("ABC"), SCORE_MAX + 1));
    EXPECT_FALSE(store.insert(static_cast<std::uint8_t>(kHighScoreGameCount), ini("ABC"), 10));
    EXPECT_EQ(store.table(kGameIdSnake).count, 0u);
}

TEST(HighScoreStore, SR_FW_09_SaveWritesTempThenFsyncThenRename) {
    FakeFileSystem fs;
    HighScoreStore store;
    store.insert(kGameIdSnake, ini("ABC"), 500);
    ASSERT_EQ(store.save(fs, kPath), SaveStatus::Ok);
    const std::vector<std::string> expected = {std::string("open ") + kTempPath, "write", "fsync",
                                               "close",
                                               std::string("rename ") + kTempPath + " " + kPath};
    EXPECT_EQ(fs.log, expected);
    EXPECT_EQ(fs.files.count(kTempPath), 0u);
    EXPECT_EQ(fs.files[kPath], "0,1,ABC,500,0\n");
}

TEST(HighScoreStore, SR_FW_09_FailedSyncLeavesOldFileUntouched) {
    FakeFileSystem fs;
    fs.files[kPath] = "0,1,OLD,1,0\n";
    fs.failSync = true;
    HighScoreStore store;
    store.insert(kGameIdSnake, ini("NEW"), 5);
    EXPECT_EQ(store.save(fs, kPath), SaveStatus::SyncFailed);
    EXPECT_EQ(fs.files[kPath], "0,1,OLD,1,0\n");
    EXPECT_EQ(fs.log.back(), "close");  // No rename after a failed fsync.
}

TEST(HighScoreStore, SR_FW_09_OpenAndWriteFailuresAreReported) {
    FakeFileSystem fs;
    HighScoreStore store;
    fs.failOpen = true;
    EXPECT_EQ(store.save(fs, kPath), SaveStatus::OpenFailed);
    fs.failOpen = false;
    fs.failWrite = true;
    EXPECT_EQ(store.save(fs, kPath), SaveStatus::WriteFailed);
    EXPECT_EQ(fs.files.count(kPath), 0u);
}

TEST(HighScoreStore, SC_03_SavedFileRoundTripsWithOnlyTableColumns) {
    FakeFileSystem fs;
    HighScoreStore store;
    store.insert(kGameIdSnake, ini("ABC"), 500);
    store.insert(kGameIdSnake, ini("DEF"), 500);
    store.insert(kGameIdBreakout, ini("XYZ"), 70);
    ASSERT_EQ(store.save(fs, kPath), SaveStatus::Ok);
    EXPECT_EQ(fs.files[kPath], "0,1,ABC,500,0\n0,2,DEF,500,1\n1,1,XYZ,70,2\n");
    HighScoreStore reloaded;
    reloaded.load(fs, kPath);
    ASSERT_EQ(reloaded.table(kGameIdSnake).count, 2u);
    EXPECT_EQ(initialsOf(reloaded.table(kGameIdSnake).rows[1]), "DEF");
    EXPECT_EQ(reloaded.table(kGameIdBreakout).rows[0].score, 70u);
}

TEST(HighScoreStore, SR_FW_09_PosixFileSystemSavesAndLoads) {
    PosixFileSystem fs;
    const std::string path = ::testing::TempDir() + "arcade_hs_test.csv";
    HighScoreStore store;
    store.insert(kGameIdSnake, ini("ABC"), 123);
    ASSERT_EQ(store.save(fs, path), SaveStatus::Ok);
    HighScoreStore reloaded;
    reloaded.load(fs, path);
    ASSERT_EQ(reloaded.table(kGameIdSnake).count, 1u);
    EXPECT_EQ(reloaded.table(kGameIdSnake).rows[0].score, 123u);
    EXPECT_FALSE(fs.readAll(path + ".tmp"));
    EXPECT_EQ(std::remove(path.c_str()), 0);
}

}  // namespace
}  // namespace arcade
