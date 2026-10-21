# Development PC setup

What you need to build and test the C++ program on a PC (SDL2 backend, UR-17) and to lint and simulate RTL in Verilator, as CI does. FPGA tools are in [`fpga-toolchain.md`](fpga-toolchain.md).

## Supported systems

- **Ubuntu 24.04 LTS** (native, or WSL2 on Windows 10/11). The commands below assume it.
- Windows without WSL2 and macOS are not supported for the full toolchain. SDL2 builds may work, but CI behaviour is defined on Ubuntu.

## 1. Packages

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git python3 \
    libsdl2-dev libgtest-dev clang-format clang-tidy gcovr \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    verilator
```

| Tool | Minimum | Used for |
|---|---|---|
| CMake | 3.25 (Ubuntu 24.04 ships 3.28) | build presets (schema v6) |
| GCC | 13 (Ubuntu 24.04 default) | PC build |
| `arm-linux-gnueabihf-g++` | Ubuntu 24.04 package | board build (HPS is ARM Cortex-A9, hard float) |
| SDL2 | 2.0 | PC backend (SR-FW-06) |
| GoogleTest | 1.14 (`libgtest-dev`) | unit tests; CMake downloads it if the package is missing |
| clang-format, clang-tidy | Ubuntu 24.04 package | `tools/lint_sw.sh` |
| gcovr | Ubuntu 24.04 package | coverage gate (QA-04) |
| Verilator | **5.020** (needs `--timing`) | RTL lint and testbenches in CI |

> **Open item (owner: Muhammad, #39):** the cross-compiler's glibc must not be newer than the glibc on the board's Linux image. Check this once the image is fixed; if it fails, #39 will pin a matching toolchain.

## 2. Verible (SystemVerilog lint and format)

Verible is not in Ubuntu's packages. CI pins release **`v0.0-4296-g0f262651`** (see `VERIBLE_TAG` in `.github/workflows/ci.yml`); use the same one, so formatting agrees with CI. Download `verible-v0.0-4296-g0f262651-linux-static-x86_64.tar.gz` from <https://github.com/chipsalliance/verible/releases/tag/v0.0-4296-g0f262651>, extract it, and put its `bin/` on your `PATH`:

```bash
verible-verilog-lint --version
verible-verilog-format --version
```

## 3. Clone and check

```bash
git clone https://github.com/Silicon-Triad/fpga-arcade-platform.git
cd fpga-arcade-platform
tools/ci_local.sh        # everything CI runs, except Quartus and the board
```

If Verible is not installed yet, `ARCADE_SKIP_VERIBLE=1 tools/ci_local.sh` skips it with a warning. CI never skips it.

Then run the games on your PC (once #16 adds `sw/app/`):

```bash
cmake --preset pc-debug && cmake --build --preset pc-debug
./build/pc-debug/sw/app/arcade --backend=sdl
```

## 4. WSL2 notes

- SDL2 windows need WSLg (Windows 11, or Windows 10 with a recent WSL). Check with `echo $DISPLAY`.
- Keep the repo inside the WSL file system (`~/...`), not under `/mnt/c`, for fast builds.
- Quartus and Questa run natively on Windows. That is fine: they read the same repo through `\\wsl$\...`, or use a second clone.
