#!/bin/sh
# CRAN-oriented build from package root (run locally from repo clone):
#   1) R CMD build .  — creates the submission .tar.gz; respects .Rbuildignore.
#      Builds the PDF reference manual (requires a working LaTeX: MacTeX,
#      TinyTeX, etc.). Do not pass --no-manual.
#   2) R CMD check --as-cran on that tarball — CRAN-like checks (includes manual).
#
# R CMD build does not support --as-cran; that flag applies to R CMD check.
#
# Usage:
#   ./tools/build-cran-submission.sh
#   sh tools/build-cran-submission.sh
set -e
ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

PKG=$(awk '/^Package:/{print $2; exit}' DESCRIPTION)
VER=$(awk '/^Version:/{print $2; exit}' DESCRIPTION)
TARBALL="${PKG}_${VER}.tar.gz"

# Optional: conservative compiler flags for build/check (does not override all
# R CMD COMPILE paths; see dev-readme.md — you may still need a safe ~/.R/Makevars).
export R_MAKEVARS_USER="$ROOT/tools/Makevars.CRAN-safe"

# Ephemeral dev dirs (gitignored or empty). Removing them avoids R CMD build
# "Removed empty directory …" chatter; demo/docs/tmp stay in the tree but are
# excluded via .Rbuildignore (R may still report empty shells for those).
for d in .Rd2pdf* tests/testthat/_snaps tests/testdata/NHIS-1965-2016; do
  if [ -d "$ROOT/$d" ]; then
    rm -rf "$ROOT/$d"
  fi
done

echo "==> R CMD build .   (PDF manual enabled — ensure pdflatex is on PATH)"
R CMD build .

if ! test -f "$TARBALL"; then
  echo "Expected $TARBALL after build (check DESCRIPTION Version)." >&2
  exit 1
fi

echo "==> R CMD check --as-cran $TARBALL"
R CMD check --as-cran "$TARBALL"

echo "Done. Submission tarball: $ROOT/$TARBALL"
