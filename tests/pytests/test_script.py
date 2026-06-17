import pytest
from pathlib import Path

from conftest import get_simulation_results
from pytest import approx
from shg_paths import PYTESTS_DIR

@pytest.fixture(scope="function")
def original_datadir(request) -> Path:
    """YAML baselines live under tests/pytests/fixtures/2018/."""
    return PYTESTS_DIR / "fixtures" / "2018"


def test_simulations(data_regression, sims):
    data_regression.check(sims)


def test_cessation_difference():
    no_immediate_cessation = get_simulation_results(cessation=0)
    immediate_cessation = get_simulation_results(cessation=2010)
    assert no_immediate_cessation != immediate_cessation


def test_exceeded_max_repeat():
    try:
        get_simulation_results(repeat=10000001)
        assert 0
    except ValueError as e:
        assert "Invalid Number of Repetitions" in str(e)


@pytest.mark.slow
@pytest.mark.RngStream
def test_2_rngs_results_should_be_different_but_approximately_the_same():
    repeat = int(1e5)
    MT = get_simulation_results(rng_strategy="MersenneTwister", repeat=repeat)
    RS = get_simulation_results(rng_strategy="RngStream", repeat=repeat)
    assert MT != RS
    rel = 1e-1
    assert MT["init_age"]["mean"] == approx(RS["init_age"]["mean"], rel=rel)
    assert MT["cess_age"]["mean"] == approx(RS["cess_age"]["mean"], rel=rel)
    assert MT["prevalence"] == approx(RS["prevalence"], rel=rel)
    assert MT["smoking_histories"]["fraction_of_switchers"] == approx(
        RS["smoking_histories"]["fraction_of_switchers"], rel=rel
    )


@pytest.mark.slow
def test_same_rng_with_different_seeds_results_should_be_different_but_approximately_the_same_MersenneTwister():
    repeat = int(1e5)
    MT1 = get_simulation_results(
        rng_strategy="MersenneTwister", repeat=repeat, MT_seed_cess=1234
    )
    MT2 = get_simulation_results(
        rng_strategy="MersenneTwister", repeat=repeat, MT_seed_cess=5678
    )
    assert MT1 != MT2
    rel = 1e-2
    assert MT1["init_age"]["mean"] == approx(MT2["init_age"]["mean"], rel=rel)
    assert MT1["cess_age"]["mean"] == approx(MT2["cess_age"]["mean"], rel=rel)
    assert MT1["prevalence"] == approx(MT2["prevalence"], rel=rel)
    assert MT1["smoking_histories"]["fraction_of_switchers"] == approx(
        MT2["smoking_histories"]["fraction_of_switchers"], rel=0.017
    )


@pytest.mark.slow
def test_same_rng_with_different_seeds_results_should_be_different_but_approximately_the_same_RngStream():
    repeat = int(1e5)
    RS1 = get_simulation_results(
        rng_strategy="RngStream",
        repeat=repeat,
        rngstream_seed="12345,12345,12345,12345,12345,12345",
    )
    RS2 = get_simulation_results(
        rng_strategy="RngStream",
        repeat=repeat,
        rngstream_seed="54321,12345,12345,12345,12345,12345",
    )
    assert RS1 != RS2
    rel = 1e-1
    assert RS1["init_age"]["mean"] == approx(RS2["init_age"]["mean"], rel=rel)
    assert RS1["cess_age"]["mean"] == approx(RS2["cess_age"]["mean"], rel=rel)
    assert RS1["prevalence"] == approx(RS2["prevalence"], rel=rel)
    assert RS1["smoking_histories"]["fraction_of_switchers"] == approx(
        RS2["smoking_histories"]["fraction_of_switchers"], rel=rel
    )


def test_not_specifying_RNGSTREAM_SEED_should_result_in_RngStream_strategy():
    repeat = int(1e4)
    RS1 = get_simulation_results(
        rng_strategy="",
        repeat=repeat,
        rngstream_seed="12345,12345,12345,12345,12345,12345",
    )
    RS2 = get_simulation_results(
        rng_strategy="RngStream",
        repeat=repeat,
        rngstream_seed="12345,12345,12345,12345,12345,12345",
    )
    RS3 = get_simulation_results(
        rng_strategy="", repeat=repeat, rngstream_seed=""
    )
    RS4 = get_simulation_results(
        rng_strategy="RngStream", repeat=repeat, rngstream_seed=""
    )
    assert RS1 == RS2
    assert RS2 == RS3
    assert RS3 == RS4


def test_not_specifying_any_MT_SEEDS_should_use_defaults():
    repeat = int(1e4)
    MT1 = get_simulation_results(rng_strategy="MersenneTwister", repeat=repeat)
    MT2 = get_simulation_results(
        rng_strategy="MersenneTwister",
        repeat=repeat,
        MT_seed_init=1898587603,
        MT_seed_cess=1468371936,
        MT_seed_mortality=1551308340,
        MT_seed_misc=1590227640,
    )
    assert MT1 == MT2


def test_not_specifying_single_MT_SEEDS_should_use_defaults():
    repeat = int(1e4)
    MT1 = get_simulation_results(
        rng_strategy="MersenneTwister",
        repeat=repeat,
        MT_seed_init=1898587603,
        MT_seed_cess=1468371936,
        MT_seed_mortality=1551308340,
        MT_seed_misc=1590227640,
    )
    MT2 = get_simulation_results(
        rng_strategy="MersenneTwister",
        repeat=repeat,
        MT_seed_init=1898587603,
        MT_seed_cess=1468371936,
        MT_seed_mortality=1551308340,
    )
    MT3 = get_simulation_results(
        rng_strategy="MersenneTwister",
        repeat=repeat,
        MT_seed_init=1898587603,
        MT_seed_cess=1468371936,
    )
    assert MT1 == MT2
    assert MT2 == MT3


@pytest.mark.parametrize(
    "rng_strategy",
    ["MersenneTwister", "RngStream"],
    ids=["MersenneTwister", "RngStream"],
)
def test_basic_characterization(rng_strategy):
    result = get_simulation_results(rng_strategy=rng_strategy)
    assert result["individuals"] >= 1000
    assert abs(result["init_age"]["min"] - 8) <= 7
    assert abs(result["cess_age"]["min"] - 15) <= 7
    assert result["death_age"]["min"] == 0

@pytest.mark.parametrize(
    "result",
    [
        get_simulation_results(
            rng_strategy="MersenneTwister", yob=1990, cessation=2010
        ),
        get_simulation_results(rng_strategy="RngStream", yob=1990, cessation=2010),
    ],
    ids=["MersenneTwister", "RngStream"],
)
def test_immediate_cessation_year(result):
    assert result["cess_age"]["max"] == 2010 - 1990 - 1
