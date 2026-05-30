#!/usr/bin/env bash
# tools/repo-cleanup.sh — bring a messy workspace back to a clean state.
#
# Wipes everything that isn't part of the project. Use this on a directory
# that's accumulated debris from earlier experiments.
#
# Run from the repo root. Safe: only removes files we recognize as cruft.
# Does NOT touch ds/ if it happens to be present locally — that's an
# expected (uncommitted) build dependency from `make ds-fetch`.

set -eu

cd "$(dirname "$0")/.."

# Drop any old project subtree that might still be lying around.
rm -rf cfmt-project/

# Old packaging artifacts.
rm -f cfmt-project.tar.gz aligner.tar.gz

# Stray binaries from earlier experiments.
rm -f aligner aligner2 destring mu x a.out

# Old single-file / experiment .c at root.
# (Excludes aligner.c at root — that's the canonical self-hosted source.)
rm -f aligner2.c aligner3.c aligner4.c alignerv2.c
rm -f al2.c alx.c asd.c asdads.c cfmt.c destring.c
rm -f fb.c fizzbuzz.c hw.c hw2.c t.c test.c xa.c xx.c

# Build artifacts + editor backups + swap files + core dumps.
make distclean >/dev/null 2>&1 || true

echo
echo "Done. The repo root now contains:"
ls -A | sort | sed 's/^/  /'
