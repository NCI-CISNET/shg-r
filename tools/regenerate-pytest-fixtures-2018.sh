#!/usr/bin/env bash
# Regenerate tests/fixtures/2018/{MT,RS}/*.txt using the installed R package (LegacyRunWebVersion).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! Rscript -e 'library(SmokingHistoryGenerator)' 2>/dev/null; then
  echo "Install SmokingHistoryGenerator first: R CMD INSTALL --preclean ." >&2
  exit 1
fi

if [[ ! -f tests/testdata/2018/csv-complete/initiation.csv ]]; then
  echo "Missing NHIS csv-complete testdata under tests/testdata/2018/csv-complete/" >&2
  exit 1
fi

export SHG_R_ROOT="$ROOT"
PYTHONPATH=tests/pytests python3 tests/pytests/test_engine_regression.py --generate-acm-rs-mt-fixtures

echo "Done: tests/fixtures/2018/"
