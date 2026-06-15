#!/usr/bin/env bash
# Run R CMD check --as-cran inside the r-hub gcc-asan container (CRAN-faithful ASAN + LTO).
#
# Usage:
#   ./tools/check-docker-asan.sh           # pull image (if needed) and run full check
#   ./tools/check-docker-asan.sh --no-pull # reuse local image
#
# Requires Docker (network for pak to fetch deps on first run). On Apple Silicon, runs the linux/amd64 image under emulation
# (same architecture CRAN gcc-ASAN uses). Expect ~15–25 minutes for this package.

set -euo pipefail

IMAGE="ghcr.io/r-hub/containers/gcc-asan:latest"
PLATFORM="linux/amd64"
DO_PULL=1

for arg in "$@"; do
  case "$arg" in
    --no-pull) DO_PULL=0 ;;
    -h|--help)
      sed -n '2,10p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg" >&2
      exit 2
      ;;
  esac
done

if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker not found. Install Docker Desktop or podman with docker CLI." >&2
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  echo "ERROR: Docker daemon is not running." >&2
  echo "Start Docker Desktop (or your Docker engine), wait until it is ready, then rerun:" >&2
  echo "  ./tools/check-docker-asan.sh" >&2
  exit 1
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PKG="$(awk '/^Package:/{print $2; exit}' DESCRIPTION)"
VER="$(awk '/^Version:/{print $2; exit}' DESCRIPTION)"
TARBALL="${PKG}_${VER}.tar.gz"

if [[ "$DO_PULL" -eq 1 ]]; then
  echo "==> Pulling $IMAGE ($PLATFORM)"
  docker pull --platform "$PLATFORM" "$IMAGE"
fi

echo "==> Building and running gcc-ASAN R CMD check in container (source mounted at /work)"
set +e
docker run --rm --platform "$PLATFORM" \
  -v "$ROOT:/work" \
  -w /work \
  -e "PKG=$PKG" \
  -e "VER=$VER" \
  "$IMAGE" \
  bash -lc '
set -euo pipefail
TARBALL="${PKG}_${VER}.tar.gz"

rm -f "${PKG}"_*.tar.gz

echo "==> Installing build dependencies (Imports, LinkingTo)"
R -q -e "
  imp <- read.dcf(\"DESCRIPTION\", fields = \"Imports\")
  lnk <- read.dcf(\"DESCRIPTION\", fields = \"LinkingTo\")
  pkgs <- unique(trimws(sub(\" \\\\(.+\", \"\", c(
    unlist(strsplit(imp, \",\")),
    unlist(strsplit(lnk, \",\"))
  ))))
  pkgs <- pkgs[nzchar(pkgs) & pkgs != \"methods\"]
  pak::pkg_install(pkgs)
"

echo "==> R CMD build ."
R CMD build --no-build-vignettes .

if [[ ! -f "$TARBALL" ]]; then
  echo "ERROR: expected $TARBALL after R CMD build" >&2
  ls -la *.tar.gz 2>/dev/null || true
  exit 1
fi

echo "==> Installing check dependencies (deps:: tarball, as r-hub r-check does)"
R -q -e "pak::pkg_install(paste0(\"deps::\", \"$TARBALL\"), dependencies = TRUE)"

echo "==> R CMD check --as-cran $CHECK_ARGS $TARBALL"
R CMD check --as-cran ${CHECK_ARGS} "$TARBALL"
'
status=$?
set -e

if [[ "$status" -ne 0 ]]; then
  echo >&2
  echo "ERROR: gcc-ASAN check failed (exit $status)." >&2
  if [[ -d "${ROOT}/${PKG}.Rcheck" ]]; then
    echo "Inspect ${ROOT}/${PKG}.Rcheck/ (especially tests/, examples/, and *.log)." >&2
  fi
  exit "$status"
fi

echo "==> gcc-ASAN check passed."
