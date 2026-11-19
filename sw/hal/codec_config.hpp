#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "hal/codec_sys.hpp"
#include "hal/register_space.hpp"

namespace arcade::hal {

// ---------------------------------------------------------------------------------------------
// I2C target (SRS §3.1.2.4 "Control interface", SR-HAL-10)
// ---------------------------------------------------------------------------------------------

/// SR-HAL-10, §3.1.2.4 "Linux access": the I2C device the HAL writes the codec through.
inline constexpr std::string_view kCodecI2cDevice = "/dev/i2c-0";
/// §3.1.2.4 "Device address": the WM8731's 7-bit I2C address (CSB low).
inline constexpr std::uint16_t kCodecI2cAddress = 0x1A;

// ---------------------------------------------------------------------------------------------
// HPS_I2C_CONTROL (SRS §3.1.2.4 "Switching the bus to the HPS")
// ---------------------------------------------------------------------------------------------

/// §3.1.2.4: HPS_I2C_CONTROL is HPS_GPIO48, bit 19 of the GPIO1 controller (DE1-SoC manual
/// §6.4). The line inside its controller.
inline constexpr unsigned kHpsI2cControlBit = 19;

/// Where the HAL finds HPS_I2C_CONTROL and the codec. Injected so tests use fake paths. The
/// board values are below; the GPIO ones were measured on the board, they are not SRS values.
struct CodecPaths {
    std::string_view gpioClassDir;   ///< Linux sysfs GPIO class directory.
    std::string_view gpioChipLabel;  ///< `label` of the gpiochip holding HPS_I2C_CONTROL.
    std::string_view i2cDevice;      ///< The codec's I2C character device (SR-HAL-10).
};

/// The standard Linux sysfs GPIO directory (kernel ABI). Measured on the DE1-SoC image,
/// 19 Nov 2026, issue #43.
inline constexpr std::string_view kGpioClassDir = "/sys/class/gpio";
/// The GPIO1 controller's gpiochip label on the board image with the #39 device tree:
/// `gpiochip483 base=483 ngpio=29 label=ff709000.gpio`, measured 19 Nov 2026, issue #43. The
/// HAL finds the chip by this label, never by a fixed GPIO number or a register address (see
/// #119: the SRS gives GPIO1 at 0xFF708000, the board shows 0xFF709000).
inline constexpr std::string_view kGpio1ChipLabel = "ff709000.gpio";

/// The paths on the board image.
inline constexpr CodecPaths kBoardCodecPaths{kGpioClassDir, kGpio1ChipLabel, kCodecI2cDevice};

// ---------------------------------------------------------------------------------------------
// WM8731 set-up (SRS §3.1.2.4 "Registers used"; values from the WM8731 datasheet the SRS cites,
// PD Rev 3.4, Table 30 "Register Map Description" unless noted)
// ---------------------------------------------------------------------------------------------

/// One codec register write: a 7-bit register address and 9 data bits (§3.1.2.4).
struct CodecWrite {
    std::uint8_t reg;    ///< Register address (R0..R15).
    std::uint16_t data;  ///< 9-bit value.
};

/// R15 Reset: writing 0 resets the device (Table 30, Reset Register).
inline constexpr CodecWrite kCodecReset{15, 0x000};
/// R6 Power Down Control = 0 0110 0111: LINEINPD, MICPD, ADCPD = 1 (line-in, mic and ADC unused,
/// §3.1.2.4); DACPD, OUTPD = 0 (DAC and line-out on); OSCPD = 1 (the FPGA drives MCLK on
/// AUD_XCK, SR-AUD-01; datasheet "OSCPD: ... The MCLK input will function independently of the
/// Oscillator being powered down"); CLKOUTPD = 1 (CLKOUT is not wired, manual Table 3-12);
/// POWEROFF = 0.
inline constexpr CodecWrite kCodecPower{6, 0x067};
/// R4 Analogue Audio Path Control = 0 0001 0010: MUTEMIC = 1 (mic unused); BYPASS = 0 (line-in
/// not passed to the output); DACSEL = 1 (DAC to the output); MICBOOST, INSEL, SIDETONE,
/// SIDEATT = 0.
inline constexpr CodecWrite kCodecAnaloguePath{4, 0x012};
/// R5 Digital Audio Path Control = 0: DACMU = 0 (soft mute off: muting is the Tone Generator's
/// job, SR-AUD-08); DEEMP = 00 (off); ADCHPD, HPOR = 0.
inline constexpr CodecWrite kCodecDigitalPath{5, 0x000};
/// R7 Digital Audio Interface Format = 0 0000 0010: FORMAT = 10 (I2S) and IWL = 00 (16 bits),
/// SR-AUD-03; MS = 0 (slave: the FPGA drives BCLK and DACLRC, SR-AUD-02); LRP, LRSWAP,
/// BCLKINV = 0.
inline constexpr CodecWrite kCodecInterfaceFormat{7, 0x002};
/// R8 Sampling Control = 0: USB/NORMAL = 0 (normal), BOSR = 0 (256 fs), SR = 0000: 48 kHz DAC
/// from a 12.288 MHz MCLK (SR-AUD-01, SR-AUD-02; datasheet Normal Mode sample-rate table, first
/// row); CLKIDIV2, CLKODIV2 = 0.
inline constexpr CodecWrite kCodecSampling{8, 0x000};
/// R9 Active Control: ACTIVE = 1, written last, after the interface and sampling registers
/// (datasheet "Activating DSP and Digital Audio Interface").
inline constexpr CodecWrite kCodecActivate{9, 0x001};

/// The SR-HAL-10 configuration, in write order: reset, power, paths, format, sampling, activate.
inline constexpr std::array<CodecWrite, 7> kCodecSetup{
    kCodecReset,           kCodecPower,    kCodecAnaloguePath, kCodecDigitalPath,
    kCodecInterfaceFormat, kCodecSampling, kCodecActivate};

/// §3.1.2.4: the two bytes on the bus for one write: register address in bits 7..1 of the first
/// byte with data bit 8 in bit 0, then data bits 7..0.
std::array<std::uint8_t, 2> encodeCodecWrite(const CodecWrite& write) noexcept;

// ---------------------------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------------------------

/// SR-HAL-10 (in the style of SR-HAL-02): one distinct code per failure. Nothing hangs or throws.
enum class CodecStatus : std::uint8_t {
    kOk,                ///< Codec configured and Audio Enable set.
    kGpioChipNotFound,  ///< No gpiochip with the label, or it is too small (needs #39's DT).
    kGpioSetupFailed,   ///< Exporting HPS_I2C_CONTROL or driving it high failed.
    kI2cOpenFailed,     ///< open(/dev/i2c-0) failed; `sysError` holds errno.
    kI2cAddressFailed,  ///< ioctl(I2C_SLAVE) failed; `sysError` holds errno.
    kCodecNoAck,        ///< A register write was not acknowledged; `reg` names it.
};

/// The outcome of configureCodec().
struct CodecResult {
    CodecStatus status = CodecStatus::kOk;
    int sysError = 0;      ///< errno of the failed call, else 0.
    std::uint8_t reg = 0;  ///< kCodecNoAck: the register that failed.
    int gpio = -1;         ///< The Linux GPIO number used for HPS_I2C_CONTROL, once found.
    bool ok() const noexcept { return status == CodecStatus::kOk; }
};

/// SR-HAL-10: configure the WM8731 and then set Audio Enable, in this order:
/// (1) find the gpiochip labelled `paths.gpioChipLabel` and take line kHpsI2cControlBit;
/// (2) export it (already exported is fine), make it an output and drive it high, which hands
///     the codec's I2C bus to the HPS (§3.1.2.4);
/// (3) open `paths.i2cDevice`, address kCodecI2cAddress, and write kCodecSetup in order,
///     stopping at the first write that is not acknowledged;
/// (4) close the device and drive HPS_I2C_CONTROL low again (the codec keeps its registers;
///     the manual's §6.4 demo does the same), then unexport it;
/// (5) only if every write was acknowledged, set Audio Control bit 0 (audio enable) through
///     `regs`, keeping the other bits as read (mute, SR-HAL-12, belongs to #44).
/// No bridge access happens on any failure, so the Tone Generator stays silent (SR-AUD-09).
/// Needs root (sysfs GPIO export and /dev/i2c-0 are root-only on the board): call it before the
/// program drops privileges (SC-01).
///
/// noexcept, yet it builds std::string paths: if memory runs out the program terminates
/// instead of returning an error; accepted because this runs once at start-up.
CodecResult configureCodec(CodecSys& sys, RegisterSpace& regs, const CodecPaths& paths) noexcept;

/// SF-02 style: the failed step of `result` as one line for the console. Empty for kOk.
std::string describeCodecFailure(const CodecResult& result, const CodecPaths& paths);

}  // namespace arcade::hal
