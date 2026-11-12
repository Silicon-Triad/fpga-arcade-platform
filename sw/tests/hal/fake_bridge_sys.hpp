#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "hal/bridge_sys.hpp"
#include "hal/dev_mem_register_space.hpp"
#include "hal/register_map.hpp"

namespace arcade::hal::test {

/// Readiness files used by the tests (fake paths: the real board values are not needed here).
inline constexpr BridgeReadiness kTestReadiness{"/fake/fpga/state", "configured",
                                                "/fake/bridge/enable", "1"};

/// Fake Linux calls for DevMemRegisterSpace (SR-HAL-01, SR-HAL-09). sysfs files are served from
/// `files`; mmap() returns a 2 MB in-memory window. Every call is recorded in `calls`, in order.
class FakeBridgeSys final : public BridgeSys {
  public:
    FakeBridgeSys() : window(BRIDGE_WINDOW_SIZE / sizeof(std::uint32_t), 0) {
        files[std::string(kTestReadiness.fpgaStatePath)] = {0, "configured\n"};
        files[std::string(kTestReadiness.bridgeEnablePath)] = {0, "1\n"};
    }

    /// Our design, loaded and ready: ID/Version = HAL_ID_MAGIC, Status FPGA ready (§3.1.4).
    void loadOurDesign() {
        poke32(kOffsetIdVersion, HAL_ID_MAGIC);
        poke32(kOffsetStatusReg, 1U << kBitStatusFpgaReady);
    }
    void poke32(std::uint32_t offset, std::uint32_t v) { window[offset / 4] = v; }
    std::uint32_t peek32(std::uint32_t offset) const { return window[offset / 4]; }
    std::uint8_t* bytes() { return reinterpret_cast<std::uint8_t*>(window.data()); }

    int readTextFile(const std::string& path, std::string& text) noexcept override {
        calls.push_back("read " + path);
        const auto it = files.find(path);
        if (it == files.end()) {
            return ENOENT;
        }
        text = it->second.second;
        return it->second.first;
    }
    int open(const std::string& path, int flags, int& err) noexcept override {
        calls.push_back("open " + path);
        openPath = path;
        openFlags = flags;
        err = openErr;
        return openErr != 0 ? -1 : kFd;
    }
    void close(int fd) noexcept override {
        calls.emplace_back("close");
        closedFds.push_back(fd);
    }
    void* mmap(std::size_t length, int prot, int flags, int fd, std::uint64_t offset,
               int& err) noexcept override {
        calls.emplace_back("mmap");
        mmapLength = length;
        mmapProt = prot;
        mmapFlags = flags;
        mmapFd = fd;
        mmapOffset = offset;
        err = mmapErr;
        return mmapErr != 0 ? nullptr : window.data();
    }
    void munmap(void* addr, std::size_t length) noexcept override {
        calls.emplace_back("munmap");
        munmapAddr = addr;
        munmapLength = length;
    }

    int count(const std::string& call) const {
        int n = 0;
        for (const auto& c : calls) {
            n += (c == call || c.rfind(call + " ", 0) == 0) ? 1 : 0;
        }
        return n;
    }

    static constexpr int kFd = 7;
    std::vector<std::uint32_t> window;                         // the bridge window, 32-bit aligned
    std::map<std::string, std::pair<int, std::string>> files;  // path -> (errno, contents)
    std::vector<std::string> calls;
    int openErr = 0;
    int mmapErr = 0;
    std::string openPath;
    int openFlags = 0;
    std::size_t mmapLength = 0;
    int mmapProt = 0;
    int mmapFlags = 0;
    int mmapFd = -1;
    std::uint64_t mmapOffset = 0;
    void* munmapAddr = nullptr;
    std::size_t munmapLength = 0;
    std::vector<int> closedFds;
};

}  // namespace arcade::hal::test
