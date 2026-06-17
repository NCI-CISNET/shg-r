"""
Build legacy SHG .txt parameter files from canonical .csv bodies for regression tests.
Comment / metadata lines (NHIS bundle style) live under tests/pytests/fixtures/nhis_legacy_txt/.
"""
from __future__ import annotations

from pathlib import Path

_TESTS_DIR = Path(__file__).resolve().parent
_FIXTURE_DIR = _TESTS_DIR / "fixtures" / "nhis_legacy_txt"


def _fixture_lines(name: str) -> tuple[str, ...]:
    p = _FIXTURE_DIR / name
    return tuple(p.read_text(encoding="utf-8").splitlines())


# Loaded once: legacy SHG doc/comment lines matching historical NHIS bundle txt files.
INIT_DOC_LINES = _fixture_lines("initiation_doc_lines.txt")
CESS_DOC_LINES = _fixture_lines("cessation_doc_lines.txt")
CPD_COMMENT_LINES = _fixture_lines("cpd_comment_block.txt")
MORT_DOC_LINES = _fixture_lines("mortality_comment_block.txt")

MORT_DIM_LINE = "1,2,1864,2100,0,99"


def _cohort_header_from_wide_csv_header(header_line: str) -> str:
    parts = [p.strip() for p in header_line.split(",")]
    if len(parts) < 4 or parts[:3] != ["RACE", "SEX", "AGE"]:
        raise ValueError(f"expected RACE,SEX,AGE,... header, got: {header_line[:80]}")
    cohort_labels = parts[3:]
    ranged = []
    for lab in cohort_labels:
        if "-" in lab:
            ranged.append(lab)
        else:
            ranged.append(f"{lab}-{lab}")
    return "Race,Sex,Age," + ",".join(ranged)


def _dimension_line_wide(num_cohorts: int) -> str:
    return f"1,2,{num_cohorts},0,99"


def wrap_initiation_csv_as_legacy_txt(csv_text: str) -> str:
    lines = [ln.strip() for ln in csv_text.strip().splitlines() if ln.strip()]
    if not lines:
        raise ValueError("empty csv")
    cohort_line = _cohort_header_from_wide_csv_header(lines[0])
    n_cohort = len(lines[0].split(",")) - 3
    dim = _dimension_line_wide(n_cohort)
    preamble = ["5", *INIT_DOC_LINES, dim, cohort_line]
    return "\n".join(preamble + lines[1:]) + "\n"


def wrap_cessation_csv_as_legacy_txt(csv_text: str) -> str:
    lines = [ln.strip() for ln in csv_text.strip().splitlines() if ln.strip()]
    if not lines:
        raise ValueError("empty csv")
    cohort_line = _cohort_header_from_wide_csv_header(lines[0])
    n_cohort = len(lines[0].split(",")) - 3
    dim = _dimension_line_wide(n_cohort)
    preamble = ["5", *CESS_DOC_LINES, dim, cohort_line]
    return "\n".join(preamble + lines[1:]) + "\n"


def wrap_cpd_csv_as_legacy_txt(csv_text: str) -> str:
    lines = [ln.strip() for ln in csv_text.strip().splitlines() if ln.strip()]
    if len(lines) < 2:
        raise ValueError("cpd csv needs header + data")
    _hdr, *body = lines
    preamble = ["7", *CPD_COMMENT_LINES, "1,2,237,0,99,6"]
    return "\n".join(preamble + body) + "\n"


def wrap_mortality_csv_as_legacy_txt(csv_text: str) -> str:
    lines = [ln.strip() for ln in csv_text.strip().splitlines() if ln.strip()]
    if len(lines) < 2:
        raise ValueError("mortality csv needs header + data")
    _hdr, *body = lines
    preamble = ["12", *MORT_DOC_LINES, MORT_DIM_LINE]
    return "\n".join(preamble + body) + "\n"
