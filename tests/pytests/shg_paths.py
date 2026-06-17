"""Path helpers for shg-r pytest suite (NHIS data + bundled extdata)."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

_PYTESTS_DIR = Path(__file__).resolve().parent
PYTESTS_DIR = _PYTESTS_DIR
REPO_ROOT = Path(os.environ.get("SHG_R_ROOT", _PYTESTS_DIR.parent.parent)).resolve()

BUNDLED_2018 = REPO_ROOT / "inst" / "extdata" / "2018"
# Full NHIS-2018 CSV tables (git-only; mirrors shg-cli data/NHIS-1965-2018 flat layout)
NHIS_CSV_COMPLETE = REPO_ROOT / "tests" / "testdata" / "2018" / "csv-complete"
NHIS_LEGACY_COMPLETE = REPO_ROOT / "tests" / "testdata" / "2018" / "legacy-complete"
RUN_FIXTURES_2018 = REPO_ROOT / "tests" / "fixtures" / "2018"


def installed_extdata_2018() -> Path:
    """Installed package extdata/2018 (smok/ + mort/); used when source tree is sparse."""
    proc = subprocess.run(
        [
            "Rscript",
            "-e",
            "cat(system.file('extdata', '2018', package='SmokingHistoryGenerator'))",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    path = Path(proc.stdout.strip())
    if not path.is_dir():
        raise FileNotFoundError(
            "Installed SmokingHistoryGenerator extdata/2018 not found; "
            "run R CMD INSTALL --preclean ."
        )
    return path


def resolve_data_paths(data_version: str = "NHIS-1965-2018") -> dict[str, str]:
    """Absolute paths for INIT/CESS/MORT/CPD tables used in legacy config files."""
    if data_version == "NHIS-1965-2018":
        if NHIS_CSV_COMPLETE.is_dir():
            base = NHIS_CSV_COMPLETE
            return {
                "init_prob": str((base / "initiation.csv").resolve()),
                "cess_prob": str((base / "cessation.csv").resolve()),
                "mortality_prob": str((base / "acm.csv").resolve()),
                "cpd_data": str((base / "cpd.csv").resolve()),
            }
        ext = installed_extdata_2018()
        return {
            "init_prob": str((ext / "smok" / "initiation.csv").resolve()),
            "cess_prob": str((ext / "smok" / "cessation.csv").resolve()),
            "mortality_prob": str((ext / "mort" / "acm.csv").resolve()),
            "cpd_data": str((ext / "smok" / "cpd.csv").resolve()),
        }
    if data_version == "bundled-2018":
        base = BUNDLED_2018 if BUNDLED_2018.is_dir() else installed_extdata_2018()
        return {
            "init_prob": str((base / "smok" / "initiation.csv").resolve()),
            "cess_prob": str((base / "smok" / "cessation.csv").resolve()),
            "mortality_prob": str((base / "mort" / "acm.csv").resolve()),
            "cpd_data": str((base / "smok" / "cpd.csv").resolve()),
        }
    raise ValueError(f"Unknown data_version: {data_version}")


def nhis_legacy_complete_paths() -> dict[str, str]:
    """Wide legacy .txt tables under tests/testdata/2018/legacy-complete/."""
    base = NHIS_LEGACY_COMPLETE
    return {
        "init_prob": str((base / "initiation.txt").resolve()),
        "cess_prob": str((base / "cessation.txt").resolve()),
        "mortality_prob": str((base / "acm.txt").resolve()),
        "cpd_data": str((base / "cpd.txt").resolve()),
    }


def nhis_legacy_wrapped_paths_from_csv() -> dict[str, str]:
    """Legacy-layout .txt built from csv-complete (full NHIS grid), matching shg-cli parity tests."""
    from legacy_nhis_format import (
        wrap_cessation_csv_as_legacy_txt,
        wrap_cpd_csv_as_legacy_txt,
        wrap_initiation_csv_as_legacy_txt,
        wrap_mortality_csv_as_legacy_txt,
    )

    if not NHIS_CSV_COMPLETE.is_dir():
        raise FileNotFoundError(f"Missing csv-complete tables: {NHIS_CSV_COMPLETE}")
    out_dir = PYTESTS_DIR / "results" / "legacy_wrapped"
    out_dir.mkdir(parents=True, exist_ok=True)

    def _write(wrap_fn, csv_name: str) -> str:
        csv_path = NHIS_CSV_COMPLETE / csv_name
        out_path = out_dir / csv_name.replace(".csv", ".txt")
        out_path.write_text(
            wrap_fn(csv_path.read_text(encoding="utf-8")), encoding="utf-8"
        )
        return str(out_path.resolve())

    return {
        "init_prob": _write(wrap_initiation_csv_as_legacy_txt, "initiation.csv"),
        "cess_prob": _write(wrap_cessation_csv_as_legacy_txt, "cessation.csv"),
        "mortality_prob": _write(wrap_mortality_csv_as_legacy_txt, "acm.csv"),
        "cpd_data": _write(wrap_cpd_csv_as_legacy_txt, "cpd.csv"),
    }
