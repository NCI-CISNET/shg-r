import hashlib
import itertools
import os
import time

import numpy as np
import pytest

from shg_paths import PYTESTS_DIR, REPO_ROOT, resolve_data_paths
from shg_r_bridge import (
    get_file_hash,
    get_package_binary_hash,
    get_worker,
    run_legacy_config,
    shutdown_worker,
)

_RESULTS_DIR = PYTESTS_DIR / "results"
_TEMPLATES_DIR = PYTESTS_DIR / "templates"


def pytest_runtest_setup():
    pass


def pytest_addoption(parser):
    parser.addoption(
        "--include-slow",
        action="store_true",
        default=False,
        help="run all tests including slow ones",
    )
    parser.addoption(
        "--legacy-only",
        action="store_true",
        default=False,
        help="exclude new tests (e.g. RngStream) from running",
    )


def pytest_configure(config):
    config.addinivalue_line("markers", "slow: mark test as slow to run")
    config.addinivalue_line("markers", "RngStream: mark test as using RngStream")
    if config.getoption("--include-slow"):
        # pytest.ini sets addopts = -m "not slow"; override so slow tests run.
        config.option.markexpr = ""


def pytest_collection_modifyitems(config, items):
    if config.getoption("--legacy-only"):
        skip_legacy = pytest.mark.skip(
            reason="remove --legacy-only flag to run new tests (e.g. RngStream)"
        )
        for item in items:
            if any("RngStream" in keyword for keyword in item.keywords):
                item.add_marker(skip_legacy)
            if any("user_input" in keyword for keyword in item.keywords):
                item.add_marker(skip_legacy)


@pytest.fixture(scope="session", autouse=True)
def shg_r_worker():
    get_worker()
    yield
    shutdown_worker()


def generate_sim_config_params():
    """Parametrize summary-stat regression runs (pytest-regressions YAML under tests/pytests/fixtures/2018/)."""
    params = []
    test_lengths = ("fast",)
    rng_strategy_list = ["MersenneTwister", "RngStream"]
    data_version_list = ["NHIS-1965-2018"]
    for test_length in test_lengths:
        race_list = [0]
        repeat_list = [1000]
        if test_length == "fast":
            sex_list = [0]
            immediate_cessation_list = [0]
            yobs = [1890, 2000, 2100]
        else:
            sex_list = [0, 1]
            immediate_cessation_list = [0, 2000, 2050]
            yobs = range(1890, 2101, 20)
        yob_count = len(yobs)
        for (
            rng_strategy,
            repeat,
            race,
            sex,
            cessation,
            data_version,
        ) in itertools.product(
            rng_strategy_list,
            repeat_list,
            race_list,
            sex_list,
            immediate_cessation_list,
            data_version_list,
        ):
            sim_config = {
                "test_length": test_length,
                "race": race,
                "sex": sex,
                "cessation": cessation,
                "data_version": data_version,
                "yobs": yobs,
                "rng_strategy": rng_strategy,
            }
            sim_id = (
                "{test_length}_race_{race}_sex_{sex}_yobs_{yob_count}_cess_{cessation}"
                "_ver_{data_version}_{rng_strategy}_repeat_{repeat}".format(
                    test_length=test_length,
                    race=race,
                    sex=sex,
                    yob_count=yob_count,
                    cessation=cessation,
                    data_version=data_version,
                    rng_strategy=rng_strategy,
                    repeat=repeat,
                )
            )
            params.append(pytest.param(sim_config, id=sim_id))
    return params


def pytest_generate_tests(metafunc):
    if "sims" in metafunc.fixturenames:
        metafunc.parametrize("sims", generate_sim_config_params(), indirect=True)


@pytest.fixture
def sims(request):
    print("Running sims with: ", request.param)
    test_length, race, sex, cessation, data_version, yobs, rng_strategy = (
        request.param.values()
    )
    sim_result = run_repeat_sims(
        race=race,
        sex=sex,
        cessation=cessation,
        data_version=data_version,
        yobs=yobs,
        rng_strategy=rng_strategy,
    )
    return sim_result


def flatten(list_of_lists):
    return [item for sublist in list_of_lists for item in sublist]


def parse_results(output_filepath):
    people = []

    start_time = time.time()
    max_duration = 30
    while not os.path.isfile(output_filepath):
        if time.time() - start_time > max_duration:
            raise ValueError(f"Output file timeout: {output_filepath}")
        time.sleep(0.5)

    with open(output_filepath, "r", encoding="utf-8") as f:
        sims_text = f.read()
    if sims_text.find("<RUN>") != -1:
        sims_text = sims_text[
            sims_text.find("<RUN>") + 5 : sims_text.find("</RUN>")
        ].split("\n")
    else:
        sims_text = sims_text.split("\n")

    sims_text = [sim.split(";")[0:-1] for sim in sims_text if len(sim) != 0]
    for sim in sims_text:
        person = {}
        person["race"] = int(sim[0])
        person["sex"] = int(sim[1])
        person["yob"] = int(sim[2])
        person["init_age"] = int(sim[3])
        person["cess_age"] = int(sim[4])
        person["death_age"] = int(sim[5])

        if person["init_age"] != -999:
            smoking_history = np.array(sim[6:])
            years = int(smoking_history.shape[0] / 2)
            history = smoking_history.reshape(years, 2)
            person["smoking_history"] = [
                [int(age), float(amount)] for age, amount in history
            ]
            person["smoking_history_string"] = ", ".join(
                [f"{item[0]}@{item[1]:.2f}" for item in person["smoking_history"]]
            )
            person["switched"] = (
                len(set([item[1] for item in person["smoking_history"]])) - 1
            )
        people.append(person)
    return people


def calculate_prevalence(people):
    people_with_smoking_history = [
        person for person in people if "smoking_history" in person.keys()
    ]
    smokers = sum(
        person["death_age"] > person["init_age"] or person["death_age"] == -999
        for person in people_with_smoking_history
    )
    return smokers / len(people)


def calculate_age_statistics(people, key):
    age_list = [int(person[key]) for person in people if int(person[key]) != -999]
    if len(age_list) == 0:
        return {"note": "All ages were -999"}
    local_min = min(age_list)
    local_max = max(age_list)
    local_median = round(float(np.median(age_list)), 4)
    local_mean = round(float(np.mean(age_list)), 4)
    return {
        "min": local_min,
        "max": local_max,
        "median": local_median,
        "mean": local_mean,
    }


def calculate_smoking_history_statistics(people):
    smoking_histories = [
        person["smoking_history"]
        for person in people
        if "smoking_history" in person.keys()
    ]
    smoking_histories_with_age_only = [
        [smoking_history_element[0] for smoking_history_element in smoking_history]
        for smoking_history in smoking_histories
    ]
    smoking_histories_with_amount_only = [
        [smoking_history_element[1] for smoking_history_element in smoking_history]
        for smoking_history in smoking_histories
    ]
    ages_of_all_histories = flatten(smoking_histories_with_age_only)
    amounts_of_all_histories = flatten(smoking_histories_with_amount_only)
    smoking_means = [
        np.mean(np.array(smoking_history_amounts))
        for smoking_history_amounts in smoking_histories_with_amount_only
    ]
    num_switchers = len([d for d in smoking_means if float(d) != int(d)])
    fraction_of_switchers = (
        round(num_switchers / len(smoking_histories), 6)
        if len(smoking_histories) > 0
        else "N/A no smoking histories"
    )

    if len(ages_of_all_histories) == 0:
        min_smoking_age, mean_smoking_age, max_smoking_age = "N/A", "N/A", "N/A"
    else:
        min_smoking_age = int(min(ages_of_all_histories))
        mean_smoking_age = round(float(np.mean(ages_of_all_histories)), 4)
        max_smoking_age = int(max(ages_of_all_histories))
    if len(amounts_of_all_histories) == 0:
        min_smoking_amount, mean_smoking_amount, max_smoking_amount = (
            "N/A",
            "N/A",
            "N/A",
        )
    else:
        min_smoking_amount = int(min(amounts_of_all_histories))
        mean_smoking_amount = round(
            float(np.mean(np.array(amounts_of_all_histories))), 4
        )
        max_smoking_amount = int(max(amounts_of_all_histories))

    return {
        "age": {
            "min": min_smoking_age,
            "mean": mean_smoking_age,
            "max": max_smoking_age,
        },
        "amount": {
            "min": min_smoking_amount,
            "mean": mean_smoking_amount,
            "max": max_smoking_amount,
        },
        "fraction_of_switchers": fraction_of_switchers,
    }


def generate_seed_hash(
    rng_strategy,
    MT_seed_init,
    MT_seed_cess,
    MT_seed_mortality,
    MT_seed_misc,
    rngstream_seed,
):
    if rng_strategy == "MersenneTwister":
        string_to_hash = (
            f"{MT_seed_init}_{MT_seed_cess}_{MT_seed_mortality}_{MT_seed_misc}"
        )
    elif rng_strategy == "RngStream":
        string_to_hash = f"{rngstream_seed}"
    elif rng_strategy == "":
        string_to_hash = f"default_{rngstream_seed}"
    else:
        raise ValueError("Unknown RNG strategy")
    return hashlib.sha256(string_to_hash.encode("utf8")).hexdigest()[:10]


def create_input_file(
    binary_hash,
    input_template_hash,
    race,
    sex,
    cessation,
    data_version,
    rng_strategy,
    yob,
    repeat,
    rngstream_seed,
    MT_seed_init,
    MT_seed_cess,
    MT_seed_mortality,
    MT_seed_misc,
    template_filename="template_input.txt",
    init_prob=None,
    cess_prob=None,
    mortality_prob=None,
    cpd_data=None,
):
    results_rng = _RESULTS_DIR / rng_strategy
    results_rng.mkdir(parents=True, exist_ok=True)

    seed_hash = generate_seed_hash(
        rng_strategy,
        MT_seed_init,
        MT_seed_cess,
        MT_seed_mortality,
        MT_seed_misc,
        rngstream_seed,
    )

    filestem = os.path.join(
        str(results_rng),
        "pkg_{binary_hash}_templ_{input_template_hash}_race_{race}_sex_{sex}_yob_{yob}_cess_{cessation}_ver_{data_version}_seedhash_{seed_hash}_rep_{repeat}".format(
            **locals()
        ),
    )
    input_filepath = filestem + "_input.txt"
    output_filepath = filestem + "_output.txt"
    error_filepath = filestem + "_error.txt"

    paths = resolve_data_paths(data_version)
    init_prob = init_prob or paths["init_prob"]
    cess_prob = cess_prob or paths["cess_prob"]
    mortality_prob = mortality_prob or paths["mortality_prob"]
    cpd_data = cpd_data or paths["cpd_data"]

    template_path = _TEMPLATES_DIR / template_filename
    open(input_filepath, "w").write(
        template_path.read_text(encoding="utf-8").format(**locals())
    )
    return input_filepath


def get_simulation_results(
    race=0,
    sex=0,
    cessation=0,
    data_version="NHIS-1965-2018",
    rng_strategy="MersenneTwister",
    yob=2050,
    repeat=1000,
    rngstream_seed="12345,12345,12345,12345,12345,12345",
    MT_seed_init=1898587603,
    MT_seed_cess=1468371936,
    MT_seed_mortality=1551308340,
    MT_seed_misc=1590227640,
    input_template_filename="template_input.txt",
):
    binary_hash = get_package_binary_hash()
    input_template_hash = get_file_hash(
        str(_TEMPLATES_DIR / input_template_filename)
    )[:8]

    input_filepath = create_input_file(
        binary_hash,
        input_template_hash,
        race,
        sex,
        cessation,
        data_version,
        rng_strategy,
        yob,
        repeat,
        rngstream_seed,
        MT_seed_init,
        MT_seed_cess,
        MT_seed_mortality,
        MT_seed_misc,
        template_filename=input_template_filename,
    )
    output_filepath = input_filepath.replace("input.txt", "output.txt")

    if not os.path.isfile(output_filepath):
        try:
            run_legacy_config(input_filepath, cwd=str(REPO_ROOT))
        except ValueError as e:
            error_filepath = input_filepath.replace("input.txt", "error.txt")
            if os.path.exists(error_filepath):
                with open(error_filepath, encoding="utf-8") as file:
                    raise ValueError("Simulation error file content: " + file.read())
            raise e

    people = parse_results(output_filepath)
    if not people:
        return []

    os.remove(input_filepath)

    first_10 = remove_attribute(people[:10], "smoking_history")
    last_10 = remove_attribute(people[-10:], "smoking_history")

    results = {
        "prevalence": calculate_prevalence(people),
        "init_age": calculate_age_statistics(people, "init_age"),
        "cess_age": calculate_age_statistics(people, "cess_age"),
        "death_age": calculate_age_statistics(people, "death_age"),
        "smoking_histories": calculate_smoking_history_statistics(people),
        "individuals": len(people),
        "sample": {"first_10": first_10, "last_10": last_10},
    }

    return results


def remove_attribute(people, attribute):
    return [
        {k: v for k, v in person.items() if k != attribute} for person in people
    ]


def run_repeat_sims(
    race=0,
    sex=1,
    cessation=0,
    data_version="NHIS-1965-2018",
    rng_strategy="MersenneTwister",
    yobs=range(1890, 2101, 20),
    repeat=1000,
    rngstream_seed="12345,12345,12345,12345,12345,12345",
    MT_seed_init=1898587603,
    MT_seed_cess=1468371936,
    MT_seed_mortality=1551308340,
    MT_seed_misc=1590227640,
):
    config = {**locals(), "yobs": ", ".join(map(str, yobs))}
    if rng_strategy == "MersenneTwister":
        config.pop("rngstream_seed", None)
    elif rng_strategy == "RngStream":
        for attribute in [
            "MT_seed_init",
            "MT_seed_cess",
            "MT_seed_mortality",
            "MT_seed_misc",
        ]:
            config.pop(attribute, None)

    summary_stats = []
    for yob in yobs:
        results = get_simulation_results(
            race,
            sex,
            cessation,
            data_version,
            rng_strategy,
            yob,
            repeat,
            rngstream_seed,
            MT_seed_init,
            MT_seed_cess,
            MT_seed_mortality,
            MT_seed_misc,
        )
        summary_stats.append({yob: results})

    return {"config": config, "summary_stats": summary_stats}
