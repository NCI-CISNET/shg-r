#!/usr/bin/env bash
# Regenerate tests/fixtures/2018/{MT,RS}/*.txt using the standalone CLI (same inputs as test-basic.R).
# Default binary: macOS SHG 6.4.0 CLI under tmp/ (not tracked; add locally or override).
# SHG 6.4.x legacy configs use OCD_PROB= / SEED_OCD= (not MORTALITY_PROB). Inputs are wide
# legacy .txt tables (see shg-cli/tmp/2018/); override SHG_FIXTURE_DATA if needed.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

DEFAULT_EXE="$ROOT/tmp/lbc_smokehist_macos_v6.4.0/lbc_smokehist_macos_v6.4.0.exe"
EXE="${SHG_LBC_EXE:-$DEFAULT_EXE}"
# SHG 6.4.x reads wide legacy .txt inputs (not the newer flat CSV layout).
DATA_PREFIX="${SHG_FIXTURE_DATA:-$ROOT/../shg-cli/tmp/NHIS-1965-2018}"
if [[ ! -f "$EXE" || ! -x "$EXE" ]]; then
  echo "Executable not found or not executable: $EXE" >&2
  echo "Place the 6.4.0 macOS binary at tmp/lbc_smokehist_macos_v6.4.0/ or set SHG_LBC_EXE." >&2
  exit 1
fi
if [[ ! -f "$DATA_PREFIX/lbc_shg_initiation.txt" ]]; then
  echo "6.4-style NHIS input bundle not found at: $DATA_PREFIX" >&2
  echo "Expected lbc_shg_initiation.txt, lbc_shg_cessation.txt, lbc_shg_cpd.txt, lbc_smokehist_ac_mortality.txt." >&2
  echo "Set SHG_FIXTURE_DATA to that directory (see shg-cli/tmp/NHIS-1965-2018)." >&2
  exit 1
fi

mkdir -p tests/outputs tests/inputs tests/fixtures/2018/MT tests/fixtures/2018/RS

write_input() {
  local rng="$1" yob="$2" cess="$3" path="$4"
  cat <<EOF >"$path"
RNGSTRATEGY=${rng}

SEED_INIT=1898587603
SEED_CESS=1468371936
SEED_OCD=1551308340
SEED_MISC=1590227640

RNGSTREAM_SEED=12345,12345,12345,12345,12345,12345

RACE=0
SEX=0
YOB=${yob}
CESSATION_YR=${cess}
REPEAT=1000

NUM_SEGMENTS=1
NUM_THREADS=1

INIT_PROB=${DATA_PREFIX}/lbc_shg_initiation.txt
CESS_PROB=${DATA_PREFIX}/lbc_shg_cessation.txt
OCD_PROB=${DATA_PREFIX}/lbc_smokehist_ac_mortality.txt
CPD_DATA=${DATA_PREFIX}/lbc_shg_cpd.txt

OUTPUTFILE=tests/outputs/test_output_${rng}_${yob}_${cess}.txt
ERRORFILE=tests/outputs/test_errors_${rng}_${yob}_${cess}.txt
EOF
}

run_case() {
  local rng="$1" yob="$2" cess="$3"
  local sub
  if [[ "$rng" == "MersenneTwister" ]]; then
    sub="MT"
  else
    sub="RS"
  fi
  local inp="tests/inputs/test_input_${rng}_${yob}_${cess}.txt"
  write_input "$rng" "$yob" "$cess" "$inp"
  "$EXE" "$inp"
  cp "tests/outputs/test_output_${rng}_${yob}_${cess}.txt" \
    "tests/fixtures/2018/${sub}/yob_${yob}_cessation_${cess}.txt"
}

run_case MersenneTwister 1950 0
run_case MersenneTwister 2010 2050
run_case RngStream 1950 0
run_case RngStream 2010 2050

echo "Wrote goldens under tests/fixtures/2018/"
