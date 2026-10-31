#!/usr/bin/env bash
# Driver for hw/sim/Makefile. Rules it enforces: AGENTS.md R-4.2.2, R-4.2.9, R-4.2.10.
#   run.sh lint | format | verilator [tb_<mod>] | questa [tb_<mod>]
# Environment overrides (for tests of this script): RTL_DIR, TB_DIR, LOG_DIR, BUILD_DIR.
# ARCADE_SKIP_VERIBLE=1 skips Verible when it is not installed (never set in CI).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
RTL_DIR="${RTL_DIR:-$repo/hw/rtl}"
TB_DIR="${TB_DIR:-$repo/hw/tb}"
LOG_DIR="${LOG_DIR:-$repo/hw/sim/logs}"
BUILD_DIR="${BUILD_DIR:-$repo/build/sim}"

shopt -s nullglob
pkgs=("$RTL_DIR"/*_pkg.sv)
rtl=()
for f in "$RTL_DIR"/*.sv; do [[ $f == *_pkg.sv ]] || rtl+=("$f"); done
tbs=("$TB_DIR"/tb_*.sv)
srcs=("${pkgs[@]}" "${rtl[@]}")

mod_of() { basename "$1" .sv; }
is_questa_only() { head -n 1 "$1" | grep -q '^// *questa-only'; }

# Pick testbenches: the one named in $1, or all of them.
select_tbs() {
    if [[ -n ${1:-} ]]; then
        local f="$TB_DIR/$1.sv"
        [[ -f $f ]] || { echo "run.sh: no testbench $f" >&2; exit 2; }
        sel=("$f")
    else
        sel=("${tbs[@]}")
    fi
}

# Verdict from simulator output: a line "PASS" (Questa prefixes "# "), no line "FAIL...",
# and no simulator error ($error/$fatal: "%Error"/"%Fatal" in Verilator, "** Error"/"** Fatal"
# in Questa), so a testbench that hits $error but still prints PASS fails (R-4.2.10).
verdict() {
    local out="$1"
    if grep -Eq '^(# )?FAIL|%(Error|Fatal)|^(# )?\*\* (Error|Fatal)' "$out"; then echo FAIL
    elif grep -Eq '^(# )?PASS$' "$out"; then echo PASS
    else echo FAIL
    fi
}

need_verible() {
    if command -v verible-verilog-lint >/dev/null && command -v verible-verilog-format >/dev/null; then
        return 0
    fi
    if [[ ${ARCADE_SKIP_VERIBLE:-0} == 1 ]]; then
        echo "run.sh: WARNING: Verible not installed, skipped (ARCADE_SKIP_VERIBLE=1)"
        return 1
    fi
    echo "run.sh: Verible not found; see docs/setup/dev-pc.md" >&2
    exit 2
}

cmd_lint() {
    if [[ ${#srcs[@]} -eq 0 && ${#tbs[@]} -eq 0 ]]; then
        echo "lint: no RTL or testbenches yet"
        return 0
    fi
    # R-4.2.2: one module per file, named after the file; lint each as its own top.
    for f in "${rtl[@]}"; do
        echo "verilator lint: $(mod_of "$f")"
        # lint.vlt waives UNUSEDPARAM in the frozen package only (see that file).
        verilator --lint-only -Wall --top-module "$(mod_of "$f")" "$here/lint.vlt" "${srcs[@]}"
    done
    # A package has no module, so lint it through a throwaway module that imports it.
    mkdir -p "$BUILD_DIR/lint"
    for f in "${pkgs[@]}"; do
        local p wrap
        p="$(mod_of "$f")"; wrap="$BUILD_DIR/lint/lint_$p.sv"
        printf 'module lint_%s;\n  import %s::*;\nendmodule\n' "$p" "$p" >"$wrap"
        echo "verilator lint: $p"
        verilator --lint-only -Wall -Wno-UNUSEDPARAM --top-module "lint_$p" "${pkgs[@]}" "$wrap"
    done
    if need_verible; then
        verible-verilog-lint "${srcs[@]}" "${tbs[@]}"
        verible-verilog-format --verify --inplace "${srcs[@]}" "${tbs[@]}"
        echo "verible: lint and format OK"
    fi
    echo "lint: OK"
}

cmd_format() {
    if [[ ${#srcs[@]} -eq 0 && ${#tbs[@]} -eq 0 ]]; then echo "format: nothing to format"; return 0; fi
    need_verible || exit 2
    verible-verilog-format --inplace "${srcs[@]}" "${tbs[@]}"
}

cmd_verilator() {
    select_tbs "${1:-}"
    if [[ ${#sel[@]} -eq 0 ]]; then echo "verilator: no testbenches yet"; return 0; fi
    local failed=0 ran=0 skipped=0
    for tb in "${sel[@]}"; do
        local name; name="$(mod_of "$tb")"
        if is_questa_only "$tb"; then
            echo "verilator: $name SKIPPED (questa-only; covered by its QuestaSim log)"
            skipped=$((skipped + 1)); continue
        fi
        local dir="$BUILD_DIR/verilator/$name"
        mkdir -p "$dir"
        verilator --binary --timing --assert -j 0 --top-module "$name" -Mdir "$dir" -o sim \
            "${srcs[@]}" "$tb" >"$dir/build.log" 2>&1 || {
            cat "$dir/build.log"; echo "verilator: $name BUILD FAILED"; failed=$((failed + 1)); continue; }
        (cd "$dir" && timeout 120 ./sim) >"$dir/run.log" 2>&1 || true
        ran=$((ran + 1))
        if [[ $(verdict "$dir/run.log") == PASS ]]; then
            echo "verilator: $name PASS"
        else
            cat "$dir/run.log"; echo "verilator: $name FAIL"; failed=$((failed + 1))
        fi
    done
    echo "verilator: $ran run, $skipped skipped, $failed failed"
    [[ $failed -eq 0 ]]
}

# Hash with CR removed, so a Windows (CRLF) checkout gives the same hash as CI (LF).
# sim_deps.py prints repo-relative paths for files inside the repo, absolute ones otherwise.
dep_path() { if [[ $1 == /* ]]; then echo "$1"; else echo "$repo/$1"; fi; }
sha() { if [[ -f $1 ]]; then tr -d '\r' <"$1" | sha256sum | cut -d' ' -f1; else echo none; fi; }

cmd_questa() {
    select_tbs "${1:-}"
    if [[ ${#sel[@]} -eq 0 ]]; then echo "questa: no testbenches yet"; return 0; fi
    command -v vsim >/dev/null || { echo "questa: vsim not on PATH; see docs/setup/fpga-toolchain.md" >&2; exit 2; }
    mkdir -p "$LOG_DIR"
    local failed=0
    for tb in "${sel[@]}"; do
        local name mod dir log
        name="$(mod_of "$tb")"; mod="${name#tb_}"
        dir="$BUILD_DIR/questa/$name"; log="$LOG_DIR/$mod.log"
        rm -rf "$dir"; mkdir -p "$dir"
        (
            cd "$dir"
            vlib work >/dev/null
            vlog -sv -work work "${srcs[@]}" "$tb"
            vsim -c -work work "$name" -do "run -all; quit -f"
        ) >"$dir/transcript.txt" 2>&1 || true
        local v; v="$(verdict "$dir/transcript.txt")"
        # Header read by tools/check_sim_logs.py: one hash per dependency (tools/sim_deps.py)
        # binds the log to the exact sources, packages and submodules it was made from.
        {
            echo "# QuestaSim log for module $mod. Generated by: make -C hw/sim questa TB=$name"
            echo "# Do not edit by hand (AGENTS.md R-4.2.10, R-10.15)."
            local dep
            while read -r dep; do
                echo "# dep: $dep sha256=$(sha "$(dep_path "$dep")")"
            done < <(python3 "$repo/tools/sim_deps.py" "$mod" --rtl-dir "$RTL_DIR" --tb-dir "$TB_DIR")
            echo "# tool: $(vsim -version 2>&1 | head -n 1)"
            echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
            cat "$dir/transcript.txt"
            echo "$v"
        } >"$log"
        echo "questa: $name $v -> ${log#"$repo"/}"
        [[ $v == PASS ]] || failed=$((failed + 1))
    done
    [[ $failed -eq 0 ]]
}

case "${1:-}" in
    lint) cmd_lint ;;
    format) cmd_format ;;
    verilator) cmd_verilator "${2:-}" ;;
    questa) cmd_questa "${2:-}" ;;
    *) echo "usage: run.sh lint | format | verilator [tb_<mod>] | questa [tb_<mod>]" >&2; exit 2 ;;
esac
