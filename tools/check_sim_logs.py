#!/usr/bin/env python3
"""Check committed QuestaSim logs (AGENTS.md section 3.4, item 3).

For every hardware module changed against the base branch (or every module with --all):
  - hw/sim/logs/<module>.log exists,
  - its last line is exactly PASS,
  - the sha256 hashes in its header match the current hw/rtl/<module>.sv and
    hw/tb/tb_<module>.sv, i.e. the log was made from the sources as they are now,
    so it is newer than the module's last change.
Packages (*_pkg.sv) have no testbench of their own and are not checked here.

Usage: tools/check_sim_logs.py [--base REF | --all]
"""
import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True).stdout


def sha256(path):
    """Hash with CR removed, matching hw/sim/run.sh, so CRLF and LF checkouts agree."""
    if not path.is_file():
        return "none"
    return hashlib.sha256(path.read_bytes().replace(b"\r", b"")).hexdigest()


def module_of(rel):
    """Map a changed path to its module name, or None if it is not a module or testbench."""
    p = Path(rel)
    if p.suffix != ".sv":
        return None
    if p.parent == Path("hw/rtl") and not p.stem.endswith("_pkg"):
        return p.stem
    if p.parent == Path("hw/tb") and p.stem.startswith("tb_"):
        return p.stem[3:]
    return None


def changed_modules(base):
    mb = git("merge-base", base, "HEAD").strip()
    paths = git("diff", "--name-only", mb, "--", "hw/rtl", "hw/tb").split()
    paths += git("ls-files", "--others", "--exclude-standard", "--", "hw/rtl", "hw/tb").split()
    return sorted({m for m in map(module_of, paths) if m})


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
    for kind, path in (("rtl", rtl), ("tb", tb)):
        m = re.search(rf"^# {kind}: \S+ sha256=(\w+)$", text, re.M)
        if not m:
            return f"log header has no {kind} hash; regenerate it with make -C hw/sim questa"
        if m.group(1) != sha256(path):
            return f"log is stale: {path.name} changed after the log was made; re-run QuestaSim"
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
            mods = changed_modules(base)
        except subprocess.CalledProcessError:
            print(f"check_sim_logs: base {base!r} not found; checking all modules")
            mods = all_modules(a.rtl_dir, a.tb_dir)

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
