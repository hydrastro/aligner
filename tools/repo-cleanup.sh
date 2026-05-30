#!/usr/bin/env bash
# tools/repo-cleanup.sh — bring a messy workspace back to a clean state.
#
# Wipes everything that isn't part of the project, optionally rescuing a
# misplaced ds/ directory from cfmt-project/vendor/.
#
# Run from the repo root. Safe: only removes files we recognize as cruft.

set -eu

cd "$(dirname "$0")/.."

# 1. If ds is still misplaced under cfmt-project/vendor/, rescue it.
if [ -d cfmt-project/vendor/ds ] && [ ! -d ds ]; then
    echo "rescuing ds/ from cfmt-project/vendor/..."
    mv cfmt-project/vendor/ds ds
fi

# 2. Drop the old project subtree wholesale.
rm -rf cfmt-project/

# 3. Old packaging artifacts.
rm -f cfmt-project.tar.gz aligner.tar.gz

# 4. Stray binaries from earlier experiments.
rm -f aligner aligner2 destring mu x a.out

# 5. Old single-file / experiment .c at root (NOT the src/ tree).
rm -f aligner.c aligner2.c aligner3.c aligner4.c alignerv2.c
rm -f al2.c alx.c asd.c asdads.c cfmt.c destring.c
rm -f fb.c fizzbuzz.c hw.c hw2.c t.c test.c xa.c xx.c

# 6. Build artifacts + editor backups + swap files + core dumps.
make distclean >/dev/null 2>&1 || true

echo
echo "Done. The repo root now contains:"
ls -A | sort | sed 's/^/  /'
