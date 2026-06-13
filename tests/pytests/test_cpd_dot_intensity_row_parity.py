"""CPD grid parity: bundled CSV / legacy layout vs tables with all-dot intensity rows dropped.

Also covers initiation-vs-CPD load errors and a small backwards-compatibility check.
RUN golden tests live in ``test_engine_regression.py``.
"""

import os
import shutil
import sys
import tempfile
from typing import List, Optional, Tuple

import pytest

_TESTS_DIR = os.path.dirname(__file__)
if _TESTS_DIR not in sys.path:
    sys.path.insert(0, _TESTS_DIR)
from legacy_nhis_format import wrap_cpd_csv_as_legacy_txt  # noqa: E402
from run_golden_helpers import extract_run_section, run_template_acm_full_output  # noqa: E402
from shg_paths import NHIS_CSV_COMPLETE, REPO_ROOT  # noqa: E402
from shg_r_bridge import run_legacy_config_result  # noqa: E402

DATA_VERSION = "NHIS-1965-2018"
_REPO_TMP = REPO_ROOT / "tmp"

CPD_FULL_NAME = "cpd.csv"


def _legacy_cpd_lines_from_canonical_csv() -> List[str]:
    """Full legacy-layout CPD lines (NHIS bundle style), built from bundled cpd.csv + test fixtures."""
    p = NHIS_CSV_COMPLETE / CPD_FULL_NAME
    with open(p, encoding="utf-8") as f:
        return wrap_cpd_csv_as_legacy_txt(f.read()).splitlines()


def _mk_test_tempdir(prefix: str) -> str:
    _REPO_TMP.mkdir(parents=True, exist_ok=True)
    return tempfile.mkdtemp(prefix=prefix, dir=str(_REPO_TMP))

NHIS_CPD_EXPECTED_LINE_COUNT = 43615
NHIS_CPD_EXPECTED_LINE_COUNT_AFTER_DOT_INTENSITY_DROP = 43615
NHIS_CPD_EXPECTED_ALL_DOT_INTENSITY_ROWS_DROPPED = 0


def _filter_cpd_drop_all_dot_intensity_rows(lines: List[str]) -> Tuple[List[str], int]:
    if not lines:
        return lines, 0
    w_first = int(lines[0].strip())
    if w_first < 2:
        raise ValueError("Invalid first line (wFirstDataLine) in CPD file")

    out = lines[:w_first]
    dropped = 0
    for line in lines[w_first:]:
        if not line.strip():
            out.append(line)
            continue
        parts = line.split(",")
        if len(parts) >= 11 and all(p.strip() == "." for p in parts[5:11]):
            dropped += 1
            continue
        out.append(line)
    return out, dropped


def _run_template_rngstream_acm_extract_run(
    yob: int, *, cpd_data_value: Optional[str] = None
) -> str:
    from run_golden_helpers import extract_run_block_substring

    full = run_template_acm_full_output(
        yob, 0, "RngStream", data_version=DATA_VERSION, cpd_data_value=cpd_data_value
    )
    return extract_run_block_substring(full)


def _write_input_file(
    path: str,
    *,
    cpd_path: str,
    output_path: str,
    error_path: str,
    seed: str = "12345,12345,12345,12345,12345,12345",
    init_path: Optional[str] = None,
) -> None:
    init_p = init_path or str(NHIS_CSV_COMPLETE / "initiation.csv")
    cess_p = str(NHIS_CSV_COMPLETE / "cessation.csv")
    mortality_p = str(NHIS_CSV_COMPLETE / "ocm-excl-lung-cancer.csv")
    content = f"""REPEAT=100
RACE=0
SEX=0
YOB=1950
CESSATION_YR=0
RNGSTRATEGY=RngStream
RNGSTREAM_SEED={seed}
NUM_THREADS=1
NUM_SEGMENTS=1
INIT_PROB={init_p}
CESS_PROB={cess_p}
MORTALITY_PROB={mortality_p}
CPD_DATA={cpd_path}
OUTPUTFILE={output_path}
ERRORFILE={error_path}
"""
    with open(path, "w") as f:
        f.write(content)


def _run_sim(input_path: str):
    return run_legacy_config_result(input_path, cwd=str(REPO_ROOT))


def test_cpd_all_dot_intensity_row_filter_line_counts():
    lines = _legacy_cpd_lines_from_canonical_csv()

    assert len(lines) == NHIS_CPD_EXPECTED_LINE_COUNT, (
        "Packaged NHIS CPD line count changed; update NHIS_CPD_EXPECTED_* constants "
        "after validating a regenerated file."
    )

    filtered, dropped = _filter_cpd_drop_all_dot_intensity_rows(lines)
    assert dropped == NHIS_CPD_EXPECTED_ALL_DOT_INTENSITY_ROWS_DROPPED
    assert len(filtered) == NHIS_CPD_EXPECTED_LINE_COUNT_AFTER_DOT_INTENSITY_DROP
    assert len(lines) - dropped == len(filtered)


@pytest.mark.RngStream
def test_cpd_full_vs_synthetic_dot_removed_same_run_output():
    if NHIS_CPD_EXPECTED_ALL_DOT_INTENSITY_ROWS_DROPPED == 0:
        pytest.skip("NHIS-2018 CPD has no all-dot intensity rows; see archive/nhis-2016-tests")
    test_dir = _mk_test_tempdir("shg_cohort_")
    try:
        full_lines = _legacy_cpd_lines_from_canonical_csv()
        filtered_lines, _ = _filter_cpd_drop_all_dot_intensity_rows(full_lines)
        cpd_synth = os.path.join(test_dir, "lbc_shg_cpd_synth_dot_removed.txt")
        with open(cpd_synth, "w") as f:
            f.write("\n".join(filtered_lines))
            f.write("\n")

        out_full = os.path.join(test_dir, "out_full.txt")
        out_synth = os.path.join(test_dir, "out_synth.txt")
        err_full = os.path.join(test_dir, "err_full.txt")
        err_synth = os.path.join(test_dir, "err_synth.txt")

        inp_full = os.path.join(test_dir, "input_full.txt")
        inp_synth = os.path.join(test_dir, "input_synth.txt")

        _write_input_file(
            inp_full,
            cpd_path=str(NHIS_CSV_COMPLETE / CPD_FULL_NAME),
            output_path=out_full,
            error_path=err_full,
        )
        _write_input_file(
            inp_synth,
            cpd_path=cpd_synth,
            output_path=out_synth,
            error_path=err_synth,
        )

        r_full = _run_sim(inp_full)
        r_synth = _run_sim(inp_synth)

        assert r_full.returncode == 0, f"Simulation failed: {r_full.stderr}"
        assert r_synth.returncode == 0, f"Simulation failed: {r_synth.stderr}"

        with open(out_full) as f:
            full_content = f.read()
        with open(out_synth) as f:
            synth_content = f.read()

        run_full = extract_run_section(full_content)
        run_synth = extract_run_section(synth_content)
        assert run_full is not None and run_synth is not None
        assert run_full == run_synth

        comb_full = (r_full.stdout or "") + (r_full.stderr or "")
        assert "[INFO] The CPD file has fewer data rows" not in comb_full

        comb_synth = (r_synth.stdout or "") + (r_synth.stderr or "")
        assert "[INFO] The CPD file has fewer data rows" in comb_synth
        assert "initiation and cessation cohort definitions" in comb_synth
        assert "no positive initiation risk" in comb_synth

    finally:
        shutil.rmtree(test_dir)


@pytest.mark.RngStream
@pytest.mark.parametrize("yob", [1950, 2010])
def test_rngstream_acm_cpd_csv_matches_dot_removed_run_yob(yob):
    if NHIS_CPD_EXPECTED_ALL_DOT_INTENSITY_ROWS_DROPPED == 0:
        pytest.skip("NHIS-2018 CPD has no all-dot intensity rows; see archive/nhis-2016-tests")
    run_csv = _run_template_rngstream_acm_extract_run(yob)
    test_dir = _mk_test_tempdir(f"shg_cpd_csv_dot_{yob}_")
    try:
        filtered_lines, _ = _filter_cpd_drop_all_dot_intensity_rows(
            _legacy_cpd_lines_from_canonical_csv()
        )
        cpd_synth = os.path.join(test_dir, "cpd_dot_removed.txt")
        with open(cpd_synth, "w", encoding="utf-8") as f:
            f.write("\n".join(filtered_lines) + "\n")
        run_dot = _run_template_rngstream_acm_extract_run(
            yob, cpd_data_value=os.path.abspath(cpd_synth)
        )
    finally:
        shutil.rmtree(test_dir, ignore_errors=True)

    assert run_csv == run_dot


@pytest.mark.RngStream
def test_reproducibility_cpd_synthetic_dot_removed():
    test_dir = _mk_test_tempdir("shg_cohort_rep_")
    try:
        filtered_lines, _ = _filter_cpd_drop_all_dot_intensity_rows(
            _legacy_cpd_lines_from_canonical_csv()
        )
        cpd_synth = os.path.join(test_dir, "lbc_shg_cpd_synth.txt")
        with open(cpd_synth, "w") as f:
            f.write("\n".join(filtered_lines))
            f.write("\n")

        seed = "12345,12345,12345,12345,12345,12345"
        out1 = os.path.join(test_dir, "out1.txt")
        out2 = os.path.join(test_dir, "out2.txt")
        inp1 = os.path.join(test_dir, "in1.txt")
        inp2 = os.path.join(test_dir, "in2.txt")

        _write_input_file(
            inp1,
            cpd_path=cpd_synth,
            output_path=out1,
            error_path=os.path.join(test_dir, "e1.txt"),
            seed=seed,
        )
        _write_input_file(
            inp2,
            cpd_path=cpd_synth,
            output_path=out2,
            error_path=os.path.join(test_dir, "e2.txt"),
            seed=seed,
        )

        r1 = _run_sim(inp1)
        r2 = _run_sim(inp2)
        assert r1.returncode == 0 and r2.returncode == 0

        with open(out1) as f:
            c1 = f.read()
        with open(out2) as f:
            c2 = f.read()

        assert extract_run_section(c1) == extract_run_section(c2)

    finally:
        shutil.rmtree(test_dir)


def _copy_init_with_positive_prob_at_age_7_male_race0(src: str, dst: str) -> None:
    with open(src) as f:
        lines = f.read().splitlines()
    out: List[str] = []
    for line in lines:
        if line.startswith("0,0,7,"):
            parts = line.split(",")
            parts[3] = "0.001"
            line = ",".join(parts)
        out.append(line)
    with open(dst, "w") as f:
        f.write("\n".join(out) + "\n")


def _cpd_legacy_with_all_dot_intensity_at_age_7(dst: str) -> None:
    lines = _legacy_cpd_lines_from_canonical_csv()
    out: List[str] = []
    for line in lines:
        if line.startswith("0,0,") and ",7," in line:
            parts = line.split(",")
            if len(parts) >= 11:
                parts[5:11] = ["."] * 6
                line = ",".join(parts)
        out.append(line)
    with open(dst, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")


@pytest.mark.RngStream
def test_cpd_load_fails_when_init_positive_without_numeric_cpd():
    test_dir = _mk_test_tempdir("shg_cpd_init_val_")
    try:
        src_init = str(NHIS_CSV_COMPLETE / "initiation.csv")
        bad_init = os.path.join(test_dir, "init_age7_positive.csv")
        _copy_init_with_positive_prob_at_age_7_male_race0(src_init, bad_init)
        bad_cpd = os.path.join(test_dir, "cpd_age7_all_dot.txt")
        _cpd_legacy_with_all_dot_intensity_at_age_7(bad_cpd)

        out_p = os.path.join(test_dir, "out.txt")
        err_p = os.path.join(test_dir, "err.txt")
        inp = os.path.join(test_dir, "input.txt")
        _write_input_file(
            inp,
            cpd_path=bad_cpd,
            output_path=out_p,
            error_path=err_p,
            init_path=bad_init,
        )
        r = _run_sim(inp)
        assert r.returncode != 0, "Simulator should refuse to start with inconsistent init vs CPD"
        with open(err_p) as f:
            err_body = f.read()
        assert "Check your CPD_DATA" in err_body
        assert "age=7" in err_body
    finally:
        shutil.rmtree(test_dir)


@pytest.mark.RngStream
def test_backwards_compatibility_default_cpd():
    from conftest import get_simulation_results

    seed = "12345,12345,12345,12345,12345,12345"

    result1 = get_simulation_results(
        race=0,
        sex=0,
        cessation=0,
        data_version=DATA_VERSION,
        rng_strategy="RngStream",
        yob=1950,
        repeat=100,
        rngstream_seed=seed,
    )

    result2 = get_simulation_results(
        race=0,
        sex=0,
        cessation=0,
        data_version=DATA_VERSION,
        rng_strategy="RngStream",
        yob=1950,
        repeat=100,
        rngstream_seed=seed,
    )

    assert result1 == result2, "Results should be identical with same seed"
