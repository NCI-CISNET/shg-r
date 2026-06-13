"""Lightweight parallel determinism checks on NHIS-2018 (default CI)."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

from shg_paths import NHIS_CSV_COMPLETE, REPO_ROOT
from shg_r_bridge import run_legacy_config

RNGSTREAM_SEED = "12345,12345,12345,12345,12345,12345"


def _run_rs(*, num_segments: int, num_threads: int, repeat: int = 200) -> list[dict]:
    with tempfile.TemporaryDirectory() as tmpdir:
        input_file = os.path.join(tmpdir, "input.txt")
        output_file = os.path.join(tmpdir, "output.txt")
        error_file = os.path.join(tmpdir, "error.txt")
        data = NHIS_CSV_COMPLETE
        content = f"""RNGSTRATEGY=RngStream
RNGSTREAM_SEED={RNGSTREAM_SEED}
NUM_SEGMENTS={num_segments}
NUM_THREADS={num_threads}
INIT_PROB={data / "initiation.csv"}
CESS_PROB={data / "cessation.csv"}
MORTALITY_PROB={data / "acm.csv"}
CPD_DATA={data / "cpd.csv"}
RACE=0
SEX=0
YOB=1950
CESSATION_YR=0
REPEAT={repeat}
OUTPUTFILE={output_file}
ERRORFILE={error_file}
"""
        with open(input_file, "w", encoding="utf-8") as f:
            f.write(content)
        run_legacy_config(input_file, cwd=str(REPO_ROOT))
        individuals = []
        with open(output_file, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("<") or line.startswith("RUNINFO"):
                    continue
                parts = line.split(";")
                if len(parts) >= 6:
                    individuals.append(
                        {
                            "race": int(parts[0]),
                            "sex": int(parts[1]),
                            "yob": int(parts[2]),
                            "init_age": int(parts[3]),
                            "cess_age": int(parts[4]),
                            "death_age": int(parts[5]),
                        }
                    )
        return individuals


def test_rs_single_segment_two_thread_counts_match() -> None:
    """Same segment count: output identical for 1 vs 4 threads (NHIS-2018)."""
    one = _run_rs(num_segments=1, num_threads=1)
    four = _run_rs(num_segments=1, num_threads=4)
    assert len(one) == len(four) == 200
    assert one == four


def test_rs_multi_segment_matches_single_segment_count() -> None:
    """Four segments (1 thread) produces the same individual count as one segment."""
    one = _run_rs(num_segments=1, num_threads=1)
    four = _run_rs(num_segments=4, num_threads=1)
    assert len(one) == len(four) == 200
