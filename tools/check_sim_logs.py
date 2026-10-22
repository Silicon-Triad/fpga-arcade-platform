#!/usr/bin/env python3
"""Check committed QuestaSim logs (AGENTS.md section 3.4, item 3).

A module is affected when any file its testbench depends on (tools/sim_deps.py: its
testbench, itself, every package and every module it instantiates) changed against the
base branch; with --all every module is checked. For each affected module:
  - hw/sim/logs/<module>.log exists,
  - its last line is exactly PASS,
  - its header has a "# dep: <path> sha256=<hash>" line for every current dependency,
    and each hash matches the file now, i.e. the log was made from the sources as they
    are, so it is newer than the last change to the module or anything it uses.
Packages (*_pkg.sv) have no testbench of their own; a package change affects every module.

Usage: tools/check_sim_logs.py [--base REF | --all]
"""
import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

from sim_deps import deps, display

REPO = Path(__file__).resolve().parent.parent


def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True).stdout


def sha256(path):
    """Hash with CR removed, matching hw/sim/run.sh, so CRLF and LF checkouts agree."""
    if not path.is_file():
        return "none"
    return hashlib.sha256(path.read_bytes().replace(b"\r", b"")).hexdigest()


def changed_files(base):
    """Repo-relative paths under hw/rtl and hw/tb that differ from the merge base."""
    mb = git("merge-base", base, "HEAD").strip()
    paths = git("diff", "--name-only", mb, "--", "hw/rtl", "hw/tb").split()
    paths += git("ls-files", "--others", "--exclude-standard", "--", "hw/rtl", "hw/tb").split()
    return set(paths)


def all_modules(rtl_dir, tb_dir):
    mods = {p.stem for p in rtl_dir.glob("*.sv") if not p.stem.endswith("_pkg")}
    mods |= {p.stem[3:] for p in tb_dir.glob("tb_*.sv")}
    return sorted(mods)


def check(mod, rtl_dir, tb_dir, log_dir):
    rtl, tb, log = rtl_dir / f"{mod}.sv", tb_dir / f"tb_{mod}.sv", log_dir / f"{mod}.log"
    if not rtl.is_file() and not tb.is_file():
        return None  # module deleted in this change
    if not tb.is_file():
        return f"no testbench hw/tb/tb_{mod}.sv"
    if not log.is_file():
        return f"no log; run: make -C hw/sim questa TB=tb_{mod}"
    text = log.read_text(errors="replace")
    lines = [ln for ln in text.splitlines() if ln.strip()]
    if not lines or lines[-1].strip() != "PASS":
        return f"log does not end in PASS (last line: {lines[-1] if lines else '<empty>'!r})"
    logged = dict(re.findall(r"^# dep: (\S+) sha256=(\w+)$", text, re.M))
    for path in deps(mod, rtl_dir, tb_dir):
        name = display(path)
        if name not in logged:
            return f"log is stale: it does not cover {name}; re-run QuestaSim"
        if logged[name] != sha256(path):
            return f"log is stale: {name} changed after the log was made; re-run QuestaSim"
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", help="base ref to diff against (default: origin/main)")
    ap.add_argument("--all", action="store_true", help="check every module, not only changed ones")
    ap.add_argument("--rtl-dir", type=Path, default=REPO / "hw/rtl")
    ap.add_argument("--tb-dir", type=Path, default=REPO / "hw/tb")
    ap.add_argument("--log-dir", type=Path, default=REPO / "hw/sim/logs")
    a = ap.parse_args()

    if a.all:
        mods = all_modules(a.rtl_dir, a.tb_dir)
    else:
        base = a.base or "origin/main"
        try:
            changed = changed_files(base)
        except subprocess.CalledProcessError:
            print(f"check_sim_logs: base {base!r} not found; checking all modules")
            mods = all_modules(a.rtl_dir, a.tb_dir)
        else:
            mods = [m for m in all_modules(a.rtl_dir, a.tb_dir)
                    if changed & {display(p) for p in deps(m, a.rtl_dir, a.tb_dir)}]

    if not mods:
        print("check_sim_logs: no hardware modules to check")
        return 0
    errors = 0
    for mod in mods:
        err = check(mod, a.rtl_dir, a.tb_dir, a.log_dir)
        print(f"check_sim_logs: {mod}: {'OK' if err is None else 'ERROR: ' + err}")
        errors += err is not None
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
