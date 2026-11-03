#!/usr/bin/env python3
"""Extract the HPS configuration of a Terasic GHRD for comparison across board revisions (#28).

  hps_params.py qsys <soc_system.qsys> [--instance hps_0]   -> sorted name=value of the HPS module
  hps_params.py qsf  <project.qsf>                          -> sorted HPS pin / I/O assignments

The .qsys format is XML: <module kind="altera_hps" name="hps_0"> holding
<parameter name="..." value="..."/> children. Output is one line per parameter, sorted, so two
revisions can be compared with `diff -u`. Values are copied verbatim (R-10.10); nothing is
interpreted or changed. Run with `python3 -I` on downloaded files.
"""
import argparse
import re
import sys
import xml.etree.ElementTree as ET


def qsys(path, instance):
    root = ET.parse(path).getroot()
    mods = [m for m in root.iter("module") if m.get("name") == instance]
    if not mods:  # fall back to the HPS kind, in case the instance has another name
        mods = [m for m in root.iter("module") if m.get("kind") == "altera_hps"]
    if len(mods) != 1:
        names = [f'{m.get("name")} ({m.get("kind")})' for m in root.iter("module")]
        sys.exit(f"{path}: expected one HPS module, found {len(mods)}. Modules: {names}")
    m = mods[0]
    print(f"# module {m.get('name')} kind={m.get('kind')} version={m.get('version')}")
    rows = []
    for p in m.findall("parameter"):
        val = (p.get("value") if p.get("value") is not None else (p.text or "")).strip()
        rows.append(f"{p.get('name')}={' '.join(val.split())}")
    for r in sorted(rows):
        print(r)
    print(f"# {len(rows)} parameters", file=sys.stderr)


def qsf(path):
    # HPS-related assignments: anything naming an HPS_* pin or the hps_* memory/io ports.
    pat = re.compile(r"\b(HPS_\w*|hps_\w*|memory_mem_\w*)", re.I)
    rows = []
    for line in open(path, errors="replace"):
        s = line.strip()
        if s and not s.startswith("#") and pat.search(s):
            rows.append(" ".join(s.split()))
    for r in sorted(set(rows)):
        print(r)
    print(f"# {len(set(rows))} HPS assignments", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("kind", choices=["qsys", "qsf"])
    ap.add_argument("path")
    ap.add_argument("--instance", default="hps_0")
    a = ap.parse_args()
    qsys(a.path, a.instance) if a.kind == "qsys" else qsf(a.path)


if __name__ == "__main__":
    main()
