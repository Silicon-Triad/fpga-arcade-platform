#!/usr/bin/env python3
"""List the source files a module's testbench depends on (AGENTS.md section 3.4, item 3).

For module <m> the dependencies are: hw/tb/tb_<m>.sv, hw/rtl/<m>.sv, every package
hw/rtl/*_pkg.sv, and every hw/rtl module that is instantiated, followed transitively.
A module counts as instantiated if its name appears as a word outside comments, which
can over-count but never misses one; an extra dependency only makes a log go stale
sooner. hw/sim/run.sh writes one hash per dependency into each QuestaSim log, and
tools/check_sim_logs.py recomputes them.

Usage: tools/sim_deps.py <module> [--rtl-dir DIR] [--tb-dir DIR]
Prints one path per line, relative to the repository root when inside it.
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
_COMMENTS = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def _words(path):
    return set(re.findall(r"\b\w+\b", _COMMENTS.sub(" ", path.read_text(errors="replace"))))


def display(path):
    """Repo-relative POSIX path when inside the repo, else absolute."""
    path = path.resolve()
    try:
        return path.relative_to(REPO).as_posix()
    except ValueError:
        return path.as_posix()


def deps(module, rtl_dir, tb_dir):
    """Return the sorted dependency paths of <module>'s testbench."""
    modules = {p.stem: p for p in rtl_dir.glob("*.sv") if not p.stem.endswith("_pkg")}
    found = set(rtl_dir.glob("*_pkg.sv"))
    todo = [p for p in (tb_dir / f"tb_{module}.sv", rtl_dir / f"{module}.sv") if p.is_file()]
    while todo:
        path = todo.pop()
        if path in found:
            continue
        found.add(path)
        for word in _words(path):
            sub = modules.get(word)
            if sub is not None and sub not in found:
                todo.append(sub)
    return sorted(found, key=display)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("module")
    ap.add_argument("--rtl-dir", type=Path, default=REPO / "hw/rtl")
    ap.add_argument("--tb-dir", type=Path, default=REPO / "hw/tb")
    a = ap.parse_args()
    for path in deps(a.module, a.rtl_dir, a.tb_dir):
        print(display(path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
