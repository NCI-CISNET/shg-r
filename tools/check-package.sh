#!/bin/bash
# Local R CMD check script for SmokingHistoryGenerator
# Run this before committing to catch issues early
#
# Usage:
#   ./tools/check-package.sh
#
# To use as a git pre-commit hook:
#   ln -sf ../../tools/check-package.sh .git/hooks/pre-commit

set -e

cd "$(dirname "$0")/.."

echo "Running R CMD check for SmokingHistoryGenerator..."
echo

# Clean previous check artifacts
rm -rf SmokingHistoryGenerator.Rcheck *.tar.gz 2>/dev/null || true

# Build the package
R CMD build . --no-manual --no-build-vignettes

# Find the tarball
TARBALL=$(ls SmokingHistoryGenerator_*.tar.gz 2>/dev/null | head -1)

if [ -z "$TARBALL" ]; then
    echo "ERROR: Failed to build package"
    exit 1
fi

# Run check
R CMD check "$TARBALL" --as-cran --no-manual

# Check for warnings
if grep -q "WARNING" SmokingHistoryGenerator.Rcheck/00check.log 2>/dev/null; then
    echo
    echo "ERROR: R CMD check found WARNINGs. Please fix before committing."
    echo
    grep -A2 "WARNING" SmokingHistoryGenerator.Rcheck/00check.log
    exit 1
fi

echo
echo "✓ R CMD check passed!"

