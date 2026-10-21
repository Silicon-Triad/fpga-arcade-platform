#!/usr/bin/env python3
"""Check committed Quartus reports in hw/reports/ (AGENTS.md section 3.4, item 4).

A PR that closes a hardware milestone (M2, M4) carries the label `hw-milestone`; CI then
runs this script with --require, so the reports must be present. In every PR, any reports
that are present must show a successful fit and met timing (no negative slack).

Usage: tools/check_reports.py [--require] [--dir hw/reports]
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def check_fit(path):
    text = path.read_text(errors="replace")
    m = re.search(r"^Fitter Status\s*:\s*(\w+)", text, re.M)
    if not m:
        return "no 'Fitter Status' line"
    if m.group(1) != "Successful":
        return f"Fitter Status is {m.group(1)}"
    return None


def check_sta(path):
    text = path.read_text(errors="replace")
    slacks = [float(s) for s in re.findall(r"^Slack\s*:\s*(-?[\d.]+)", text, re.M)]
    if not slacks:
        return "no 'Slack' lines"
    worst = min(slacks)
    if worst < 0:
        return f"timing not met: worst slack {worst} ns"
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--require", action="store_true", help="fail if the reports are missing")
    ap.add_argument("--dir", type=Path, default=REPO / "hw/reports")
    a = ap.parse_args()

    fits = sorted(a.dir.glob("*.fit.summary"))
    stas = sorted(a.dir.glob("*.sta.summary"))
    errors = []
    if a.require and not fits:
        errors.append("missing *.fit.summary; run tools/collect_reports.sh after a full Quartus build")
    if a.require and not stas:
        errors.append("missing *.sta.summary; run tools/collect_reports.sh after a full Quartus build")
    if bool(fits) != bool(stas):
        errors.append("fit and timing summaries must be committed together")
    for path, fn in [(p, check_fit) for p in fits] + [(p, check_sta) for p in stas]:
        err = fn(path)
        print(f"check_reports: {path.name}: {'OK' if err is None else 'ERROR: ' + err}")
        if err:
            errors.append(f"{path.name}: {err}")
    for e in errors:
        print(f"check_reports: ERROR: {e}")
    if not fits and not stas and not errors:
        print("check_reports: no reports (only required on hw-milestone PRs)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
