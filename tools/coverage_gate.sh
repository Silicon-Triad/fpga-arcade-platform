#!/usr/bin/env bash
# QA-04: fail when line coverage of sw/engine/ plus sw/games/ is below the threshold.
# gcovr decides pass or fail; this script only prints it as one line for the CI log.
#   tools/coverage_gate.sh <gcovr> <source root> <build dir> <threshold %>
# Called by the `coverage` target in sw/CMakeLists.txt, which holds the threshold.
set -uo pipefail

gcovr=$1 root=$2 build=$3 threshold=$4

summary=$("$gcovr" --root "$root" --filter "$root/sw/engine/" --filter "$root/sw/games/" \
    --exclude "$root/sw/tests/" --print-summary --fail-under-line "$threshold" "$build")
status=$?

line=$(grep '^lines:' <<<"$summary")
# Zero measured lines means the filters or the data are wrong, not that the code is covered.
if [[ -z $line || $line == *"(0 out of 0)"* ]]; then
    echo "QA-04 coverage gate: FAIL, no coverage result for sw/engine + sw/games" >&2
    exit 1
fi

verdict=PASS
[[ $status == 0 ]] || verdict=FAIL
echo "QA-04 coverage gate: sw/engine + sw/games ${line}, threshold ${threshold}%: ${verdict}"
exit "$status"
