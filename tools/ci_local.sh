#!/usr/bin/env bash
# Run locally what CI runs on every PR (AGENTS.md section 3.3), except the Quartus
# build and the board. Run before every push.
#   tools/ci_local.sh [--sw-only | --hw-only]
set -euo pipefail
cd "$(dirname "$0")/.."

do_sw=1 do_hw=1
case "${1:-}" in
    --sw-only) do_hw=0 ;;
    --hw-only) do_sw=0 ;;
    "") ;;
    *) echo "usage: $0 [--sw-only | --hw-only]" >&2; exit 2 ;;
esac

step() { printf '\n=== %s ===\n' "$*"; }

if [[ $do_sw == 1 ]]; then
    step "sw: build and unit tests (pc-debug)"
    cmake --preset pc-debug
    cmake --build --preset pc-debug
    ctest --preset pc-debug

    step "sw: coverage report (pc-coverage)"
    cmake --preset pc-coverage
    cmake --build --preset pc-coverage
    ctest --preset pc-coverage
    cmake --build --preset pc-coverage --target coverage

    step "sw: cross build for the board (de1soc-release)"
    if command -v arm-linux-gnueabihf-g++ >/dev/null; then
        cmake --preset de1soc-release
        cmake --build --preset de1soc-release
    else
        echo "WARNING: arm-linux-gnueabihf-g++ not installed; skipped (CI runs it)"
    fi

    step "sw: format and static analysis"
    tools/lint_sw.sh --format-check
    tools/lint_sw.sh --tidy

    step "sw: layering check (QA-01)"
    if [[ -f tools/check_includes.py ]]; then
        python3 tools/check_includes.py
    else
        echo "not yet implemented (issue #5)"
    fi
fi

if [[ $do_hw == 1 ]]; then
    step "hw: lint"
    make -s -C hw/sim lint

    step "hw: testbenches in Verilator"
    make -s -C hw/sim verilator

    step "hw: QuestaSim logs for changed modules"
    python3 tools/check_sim_logs.py

    step "hw: Quartus reports"
    python3 tools/check_reports.py
fi

printf '\n=== ci_local: ALL PASSED ===\n'
