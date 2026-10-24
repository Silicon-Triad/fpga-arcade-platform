// Tests of the real inotify calls in LinuxEvdevSys on a temporary directory (issue #22,
// SR-INP-15). No input devices are needed.
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "input/evdev_sys.hpp"

namespace arcade {
namespace {

/// Records what readDirChanges() reports.
class RecordingSink final : public DirChangeSink {
  public:
    std::vector<std::string> names;
    int lost = 0;
    void entryChanged(const char* name) override { names.emplace_back(name); }
    void eventsLost() override { ++lost; }
};

class LinuxWatchTest : public ::testing::Test {
  protected:
    void SetUp() override {
        std::string tmpl = (std::filesystem::temp_directory_path() / "arcade-inotify-XXXXXX");
        ASSERT_NE(::mkdtemp(tmpl.data()), nullptr);
        dir = tmpl;
        fd = sys.watchDir(dir);
        ASSERT_GE(fd, 0);
    }
    void TearDown() override {
        if (fd >= 0) {
            sys.close(fd);
        }
        std::filesystem::remove_all(dir);
    }
    void touch(const std::string& name) { std::ofstream(dir + "/" + name).put('x'); }
    std::vector<std::string> changes() {
        RecordingSink sink;
        sys.readDirChanges(fd, sink);
        EXPECT_EQ(sink.lost, 0);
        return sink.names;
    }

    LinuxEvdevSys sys;
    std::string dir;
    int fd = -1;
};

TEST_F(LinuxWatchTest, SR_INP_15_NothingChangedReportsNothing) { EXPECT_TRUE(changes().empty()); }

TEST_F(LinuxWatchTest, SR_INP_15_NewEntryIsReportedByName) {
    touch("event7");  // IN_CREATE (writing it may add more events for the same name)
    const auto names = changes();
    ASSERT_FALSE(names.empty());
    for (const auto& n : names) {
        EXPECT_EQ(n, "event7");
    }
    EXPECT_TRUE(changes().empty());  // drained
}

TEST_F(LinuxWatchTest, SR_INP_15_ModeChangeIsReportedByName) {
    touch("event7");
    changes();
    ASSERT_EQ(::chmod((dir + "/event7").c_str(), S_IRUSR | S_IWUSR | S_IRGRP), 0);  // IN_ATTRIB
    EXPECT_EQ(changes(), std::vector<std::string>{"event7"});
}

TEST_F(LinuxWatchTest, SR_INP_15_ManyEventsAreAllDrained) {
    for (int i = 0; i < 200; ++i) {  // more than one read() buffer of events
        ASSERT_EQ(::mkdir((dir + "/event" + std::to_string(i)).c_str(), S_IRWXU), 0);
    }
    const auto names = changes();
    ASSERT_EQ(names.size(), 200U);  // one IN_CREATE each, none lost or cut
    EXPECT_EQ(names.front(), "event0");
    EXPECT_EQ(names.back(), "event199");
    EXPECT_TRUE(changes().empty());
}

TEST(LinuxWatch, SR_INP_15_MissingDirectoryFails) {
    LinuxEvdevSys sys;
    EXPECT_LT(sys.watchDir("/nonexistent/arcade-inotify-test"), 0);
}

}  // namespace
}  // namespace arcade
