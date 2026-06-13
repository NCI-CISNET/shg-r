"""Shared helpers for NHIS-2018 RUN golden fixtures."""

from __future__ import annotations

import os
from typing import List, Optional, Tuple

from conftest import create_input_file, get_file_hash, get_package_binary_hash
from shg_paths import REPO_ROOT, RUN_FIXTURES_2018
from shg_r_bridge import run_legacy_config

_TESTS_DIR = os.path.dirname(__file__)
_TEMPLATE_INPUT = "template_input.txt"
_RNGSTREAM_SEED_DEFAULT = "12345,12345,12345,12345,12345,12345"
_MT_SEED_INIT = 1898587603
_MT_SEED_CESS = 1468371936
_MT_SEED_MORTALITY = 1551308340
_MT_SEED_MISC = 1590227640
_REPEAT_ACM_REGRESSION = 1000

ACM_RS_MT_SCENARIOS: Tuple[Tuple[str, str, int, int], ...] = (
    ("RS", "RngStream", 1950, 0),
    ("MT", "MersenneTwister", 1950, 0),
    ("RS", "RngStream", 2010, 2050),
    ("MT", "MersenneTwister", 2010, 2050),
)


def extract_run_section(content: str) -> Optional[str]:
    lines = content.split("\n")
    run_start = None
    run_end = None
    for i, line in enumerate(lines):
        if "<RUN>" in line:
            run_start = i
        if "</RUN>" in line and run_start is not None:
            run_end = i
            break
    if run_start is not None and run_end is not None:
        return "\n".join(lines[run_start : run_end + 1])
    return None


def normalize_run_block(text: str) -> str:
    """Strip BOM/CRLF so RUN comparisons match across Windows and Unix (shg-r test-basic.R)."""
    if text.startswith("\ufeff"):
        text = text[1:]
    return text.replace("\r\n", "\n").replace("\r", "")


def extract_run_block_substring(content: str) -> str:
    start = content.find("<RUN>")
    end = content.find("</RUN>")
    assert start >= 0 and end > start, "Missing <RUN>...</RUN> in simulator output"
    return normalize_run_block(content[start : end + len("</RUN>")])


def shg_r_style_fixture_path(
    fixture_year: str,
    rng_folder: str,
    yob: int,
    cessation: int,
) -> str:
    assert rng_folder in ("RS", "MT"), rng_folder
    name = f"yob_{yob}_cessation_{cessation}.txt"
    return os.path.join(RUN_FIXTURES_2018, rng_folder, name)


def set_cpd_data_line(input_filepath: str, cpd_data_line_body: str) -> None:
    with open(input_filepath, encoding="utf-8") as f:
        lines = f.read().splitlines()
    out: List[str] = []
    replaced = False
    for line in lines:
        if line.startswith("CPD_DATA="):
            out.append(f"CPD_DATA={cpd_data_line_body}")
            replaced = True
        else:
            out.append(line)
    assert replaced, f"No CPD_DATA= line in {input_filepath}"
    with open(input_filepath, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")


def run_template_acm_full_output(
    yob: int,
    cessation: int,
    rng_strategy: str,
    *,
    data_version: str,
    cpd_data_value: Optional[str] = None,
    init_prob: Optional[str] = None,
    cess_prob: Optional[str] = None,
    mortality_prob: Optional[str] = None,
) -> str:
    """Full simulator output for template_input + NHIS data + acm mortality."""
    binary_hash = get_package_binary_hash()
    template_hash = get_file_hash(
        os.path.join(_TESTS_DIR, "templates", _TEMPLATE_INPUT)
    )[:8]
    input_filepath = create_input_file(
        binary_hash,
        template_hash,
        0,
        0,
        cessation,
        data_version,
        rng_strategy,
        yob,
        _REPEAT_ACM_REGRESSION,
        _RNGSTREAM_SEED_DEFAULT,
        _MT_SEED_INIT,
        _MT_SEED_CESS,
        _MT_SEED_MORTALITY,
        _MT_SEED_MISC,
        template_filename=_TEMPLATE_INPUT,
        init_prob=init_prob,
        cess_prob=cess_prob,
        mortality_prob=mortality_prob,
        cpd_data=cpd_data_value,
    )
    if cpd_data_value is not None and init_prob is None:
        set_cpd_data_line(input_filepath, cpd_data_value)
    if init_prob is not None or cess_prob is not None or mortality_prob is not None:
        _replace_datafile_lines(
            input_filepath,
            init_prob=init_prob,
            cess_prob=cess_prob,
            mortality_prob=mortality_prob,
        )

    run_legacy_config(input_filepath, cwd=str(REPO_ROOT))
    output_filepath = input_filepath.replace("input.txt", "output.txt")
    with open(output_filepath, encoding="utf-8") as f:
        return f.read()


def _replace_datafile_lines(
    input_filepath: str,
    *,
    init_prob: Optional[str] = None,
    cess_prob: Optional[str] = None,
    mortality_prob: Optional[str] = None,
) -> None:
    mapping = {
        "INIT_PROB=": init_prob,
        "CESS_PROB=": cess_prob,
        "MORTALITY_PROB=": mortality_prob,
    }
    with open(input_filepath, encoding="utf-8") as f:
        lines = f.read().splitlines()
    out: List[str] = []
    for line in lines:
        replaced = False
        for prefix, value in mapping.items():
            if value is not None and line.startswith(prefix):
                out.append(f"{prefix}{value}")
                replaced = True
                break
        if not replaced:
            out.append(line)
    with open(input_filepath, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")


def write_acm_rs_mt_fixtures(fixture_year: str, data_version: str) -> None:
    for folder, rng, yob, cess in ACM_RS_MT_SCENARIOS:
        path = shg_r_style_fixture_path(fixture_year, folder, yob, cess)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        body = run_template_acm_full_output(
            yob, cess, rng, data_version=data_version
        )
        with open(path, "w", encoding="utf-8") as f:
            f.write(body)
        print("Wrote", path)
