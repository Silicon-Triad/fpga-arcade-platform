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

#include "input/evdev_sys.hpp"

namespace arcade {
namespace {

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

    LinuxEvdevSys sys;
    std::string dir;
    int fd = -1;
};

TEST_F(LinuxWatchTest, SR_INP_15_NothingChangedGivesFalse) { EXPECT_FALSE(sys.dirChanged(fd)); }

TEST_F(LinuxWatchTest, SR_INP_15_NewEntryIsReported) {
    touch("event7");  // IN_CREATE
    EXPECT_TRUE(sys.dirChanged(fd));
    EXPECT_FALSE(sys.dirChanged(fd));  // drained
}

TEST_F(LinuxWatchTest, SR_INP_15_ModeChangeIsReported) {
    touch("event7");
    ASSERT_TRUE(sys.dirChanged(fd));
    ASSERT_EQ(::chmod((dir + "/event7").c_str(), S_IRUSR | S_IWUSR | S_IRGRP), 0);  // IN_ATTRIB
    EXPECT_TRUE(sys.dirChanged(fd));
}

TEST_F(LinuxWatchTest, SR_INP_15_ManyEventsAreAllDrained) {
    for (int i = 0; i < 200; ++i) {  // more than one read() buffer of events
        touch("event" + std::to_string(i));
    }
    EXPECT_TRUE(sys.dirChanged(fd));
    EXPECT_FALSE(sys.dirChanged(fd));
}

TEST(LinuxWatch, SR_INP_15_MissingDirectoryFails) {
    LinuxEvdevSys sys;
    EXPECT_LT(sys.watchDir("/nonexistent/arcade-inotify-test"), 0);
}

}  // namespace
}  // namespace arcade
