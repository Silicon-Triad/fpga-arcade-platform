// codec_setup (#43): runs the HAL's SR-HAL-10 codec configuration on the board and reports
// each result, so it can be checked without the whole arcade program.
//
// Board only, as root, after arcade_top is loaded (load_fpga.sh) and with the #39 device tree
// (/dev/i2c-0 and the ff709000.gpio chip present). It first maps and checks the bridge
// (DevMemRegisterSpace::open(), SR-HAL-09) and touches neither the codec nor the bridge if that
// fails. Then it runs configureCodec(): HPS_I2C_CONTROL high, the WM8731 writes, the bus back to
// the FPGA side, and Audio Enable.
//
// Exit status: 0 configured, 3 bridge not ready, 4 codec configuration error.

#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

#include "hal/codec_config.hpp"
#include "hal/codec_sys.hpp"
#include "hal/dev_mem_register_space.hpp"
#include "hal/register_map.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitBridgeNotReady = 3;
constexpr int kExitCodecError = 4;

std::string hex32(std::uint32_t v) {
    std::array<char, 11> buf{};
    static_cast<void>(std::snprintf(buf.data(), buf.size(), "0x%08X", v));
    return {buf.data()};
}

}  // namespace

int main() {
    using namespace arcade::hal;

    LinuxBridgeSys bridgeSys;
    DevMemRegisterSpace regs(bridgeSys, kBoardReadiness);
    const BridgeResult bridge = regs.open();
    if (!bridge.ok()) {
        std::cerr << "graphics hardware not ready: "
                  << describeBridgeFailure(bridge, regs.readiness()) << "\n";
        return kExitBridgeNotReady;
    }
    std::cout << "bridge: mapped, ID/Version and FPGA ready OK\n"
              << "Audio Control before: " << hex32(regs.read32(kOffsetAudioControl)) << "\n";

    LinuxCodecSys codecSys;
    const CodecResult codec = configureCodec(codecSys, regs, kBoardCodecPaths);
    std::cout << "HPS_I2C_CONTROL: GPIO " << codec.gpio << "\n";
    if (!codec.ok()) {
        std::cerr << describeCodecFailure(codec, kBoardCodecPaths) << "\n";
        regs.release();
        return kExitCodecError;
    }
    for (const CodecWrite& w : kCodecSetup) {
        const auto bytes = encodeCodecWrite(w);
        std::array<char, 48> line{};
        static_cast<void>(
            std::snprintf(line.data(), line.size(), "  R%-2u = 0x%03X  bytes %02X %02X  ACK\n",
                          static_cast<unsigned>(w.reg), static_cast<unsigned>(w.data),
                          static_cast<unsigned>(bytes[0]), static_cast<unsigned>(bytes[1])));
        std::cout << line.data();
    }
    std::cout << "Audio Control after:  " << hex32(regs.read32(kOffsetAudioControl)) << "\n"
              << "codec configured (SR-HAL-10)\n";
    regs.release();
    return kExitOk;
}
