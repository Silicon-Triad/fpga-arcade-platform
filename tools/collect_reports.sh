#!/usr/bin/env bash
# Copy the fit and timing summaries of the last full Quartus build into hw/reports/
# (AGENTS.md section 3.2). Run after: quartus_sh --flow compile hw/quartus/arcade
set -euo pipefail
cd "$(dirname "$0")/.."
out=hw/quartus/output_files
shopt -s nullglob
files=("$out"/*.fit.summary "$out"/*.sta.summary)
if [[ ${#files[@]} -eq 0 ]]; then
    echo "collect_reports: nothing in $out; run a full Quartus build first" >&2
    exit 1
fi
mkdir -p hw/reports
cp -v "${files[@]}" hw/reports/
python3 tools/check_reports.py --require
