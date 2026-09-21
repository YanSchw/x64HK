#!/bin/sh
#
# Normalise source indentation to spaces.
#
# Every tab is expanded to the next TAB_WIDTH column boundary rather than being
# blindly replaced by N spaces. That distinction matters: tabs are used mid-line
# to align trailing comments, and only tab-stop expansion keeps those columns
# where they were.
#
# Makefiles are deliberately skipped. make requires a literal tab at the start
# of every recipe line, so expanding those silently breaks the build.

set -eu

TAB_WIDTH=4
CHECK=0

usage() {
    cat <<USAGE
Usage: Tools/Untabify.sh [--check] [--width N] [path ...]

  --check     list files that would change without touching them (exit 1 if any)
  --width N   tab stop width (default ${TAB_WIDTH})

Operates on .h .cpp .asm .inc .ld files. Defaults to the whole repository.
Build output, .git and Makefiles are always skipped.
USAGE
}

while [ $# -gt 0 ]; do
    case "$1" in
        --check) CHECK=1; shift ;;
        --width) TAB_WIDTH="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        --) shift; break ;;
        -*) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) break ;;
    esac
done

[ $# -gt 0 ] || set -- .

list=$(mktemp)
result=$(mktemp)
trap 'rm -f "$list" "$result"' EXIT INT TERM

find "$@" \
    \( -name Build -o -name .git -o -name '*.mk' -o -name Makefile \) -prune -o \
    -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.asm' -o -name '*.inc' -o -name '*.ld' \) \
    -print | LC_ALL=C sort > "$list"

changed=0
scanned=0

while IFS= read -r file; do
    scanned=$((scanned + 1))

    # Expand tabs, then drop the trailing whitespace expansion can leave behind
    # on a line that ended in one.
    expand -t "$TAB_WIDTH" "$file" | sed -e 's/[[:space:]]*$//' > "$result"

    if cmp -s "$file" "$result"; then
        continue
    fi

    changed=$((changed + 1))
    if [ "$CHECK" -eq 1 ]; then
        echo "would change  $file"
    else
        # Write through the existing file so permissions and inode survive.
        cat "$result" > "$file"
        echo "untabified    $file"
    fi
done < "$list"

if [ "$CHECK" -eq 1 ]; then
    echo "$changed of $scanned files still contain tabs"
    [ "$changed" -eq 0 ] || exit 1
else
    echo "$changed of $scanned files rewritten"
fi
