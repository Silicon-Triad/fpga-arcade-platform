#!/usr/bin/env bash
# C++ format and static-analysis checks for sw/ (AGENTS.md section 3.1).
#   tools/lint_sw.sh --format-check   fail if any file is not clang-formatted
#   tools/lint_sw.sh --format-fix     reformat files in place
#   tools/lint_sw.sh --tidy           run clang-tidy (needs the pc-debug build)
set -euo pipefail
cd "$(dirname "$0")/.."

mapfile -t files < <(git ls-files --cached --others --exclude-standard -- \
    'sw/*.cpp' 'sw/*.hpp' 'sw/*.h' 'sw/*.cc')

if [[ ${#files[@]} -eq 0 ]]; then
    echo "lint_sw: no C++ files yet"
    exit 0
fi

case "${1:-}" in
    --format-check)
        clang-format --dry-run --Werror "${files[@]}"
        echo "lint_sw: format OK (${#files[@]} files)"
        ;;
    --format-fix)
        clang-format -i "${files[@]}"
        echo "lint_sw: formatted ${#files[@]} files"
        ;;
    --tidy)
        db=build/pc-debug/compile_commands.json
        if [[ ! -f $db ]]; then
            echo "lint_sw: $db missing; run: cmake --preset pc-debug" >&2
            exit 2
        fi
        mapfile -t sources < <(printf '%s\n' "${files[@]}" | grep -E '\.(cpp|cc)$' || true)
        [[ ${#sources[@]} -eq 0 ]] && { echo "lint_sw: no sources for clang-tidy"; exit 0; }
        clang-tidy -p build/pc-debug --quiet "${sources[@]}"
        echo "lint_sw: clang-tidy OK (${#sources[@]} files)"
        ;;
    *)
        echo "usage: $0 --format-check | --format-fix | --tidy" >&2
        exit 2
        ;;
esac
