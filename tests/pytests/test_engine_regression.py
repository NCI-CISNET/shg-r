"""Engine regression: NHIS-2018 RUN goldens and CSV vs legacy .txt parity."""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

_TESTS_DIR = Path(__file__).resolve().parent
if str(_TESTS_DIR) not in sys.path:
    sys.path.insert(0, str(_TESTS_DIR))

from run_golden_helpers import (  # noqa: E402
    ACM_RS_MT_SCENARIOS,
    extract_run_block_substring,
    run_template_acm_full_output,
    shg_r_style_fixture_path,
)
from shg_paths import nhis_legacy_wrapped_paths_from_csv  # noqa: E402

_DATA_VERSION = "NHIS-1965-2018"
_FIXTURE_YEAR = "2018"


def _scenario_params():
    out = []
    for folder, rng_strategy, yob, cess in ACM_RS_MT_SCENARIOS:
        marks = [pytest.mark.RngStream] if rng_strategy == "RngStream" else []
        out.append(
            pytest.param(
                folder,
                rng_strategy,
                yob,
                cess,
                marks=marks,
                id=f"{folder}_y{yob}_cess{cess}",
            )
        )
    return out


@pytest.mark.parametrize("folder,rng_strategy,yob,cess", _scenario_params())
def test_acm_rs_mt_matches_fixture(
    folder: str,
    rng_strategy: str,
    yob: int,
    cess: int,
) -> None:
    """``<RUN>…</RUN>`` matches tests/fixtures/2018/{RS|MT}/ (NHIS-1965-2018 CSV inputs)."""
    path = shg_r_style_fixture_path(_FIXTURE_YEAR, folder, yob, cess)
    assert os.path.isfile(path), f"Missing committed fixture: {path}"
    with open(path, encoding="utf-8") as f:
        expected = f.read()
    got = run_template_acm_full_output(
        yob, cess, rng_strategy, data_version=_DATA_VERSION
    )
    assert extract_run_block_substring(got) == extract_run_block_substring(expected), (
        f"RUN block drift vs {path} — run tools/regenerate-pytest-fixtures-2018.sh"
    )


@pytest.mark.parametrize("folder,rng_strategy,yob,cess", _scenario_params())
def test_csv_matches_legacy_txt_run(
    folder: str,
    rng_strategy: str,
    yob: int,
    cess: int,
) -> None:
    """Bundled CSV and wide legacy .txt under tests/testdata yield the same <RUN>."""
    legacy = nhis_legacy_wrapped_paths_from_csv()
    run_csv = run_template_acm_full_output(
        yob, cess, rng_strategy, data_version=_DATA_VERSION
    )
    run_txt = run_template_acm_full_output(
        yob,
        cess,
        rng_strategy,
        data_version=_DATA_VERSION,
        init_prob=legacy["init_prob"],
        cess_prob=legacy["cess_prob"],
        mortality_prob=legacy["mortality_prob"],
        cpd_data_value=legacy["cpd_data"],
    )
    assert extract_run_block_substring(run_csv) == extract_run_block_substring(run_txt)


if __name__ == "__main__":
    if "--generate-acm-rs-mt-fixtures" in sys.argv:
        from run_golden_helpers import write_acm_rs_mt_fixtures

        write_acm_rs_mt_fixtures(_FIXTURE_YEAR, _DATA_VERSION)
