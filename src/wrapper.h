#define STRICT_R_HEADERS
#include <Rcpp.h>
#include "smoking_sim.h"

class SHGInterface {
public:
    SHGInterface();
    // Function to run simulations in parallel and combine results
    bool isValidDataFrame(Rcpp::DataFrame& dfPopulation);
    Rcpp::DataFrame runSimFromFixedValues(int repeat, short wRace, short wSex, short wYearBirth);
    Rcpp::DataFrame runSimFromDataFrame(Rcpp::DataFrame dfPopulation);

    void initialize();
    void LegacyRunWebVersion(const char *sInputFileName);
    const char *sInputFile;
    const char *sOutputFile;
    Smoking_Simulator *pSimulator = 0;
    Smoking_Simulator* createSimulator();

    int number_of_segments = 10; // TODO: maybe default value should be set in constructor instead?
    bool run_multi_threaded = true; // TODO: maybe default value should be set in constructor instead?
    std::string rng_strategy = "RngStream"; // TODO: maybe default value should be set in constructor instead?

    // Getters and Setters
    int get_number_of_segments() {return number_of_segments;};
    void set_number_of_segments(int n) {number_of_segments = n;};

    bool get_run_multi_threaded() {return run_multi_threaded;};
    void set_run_multi_threaded(bool b) {run_multi_threaded = b;};

    std::string get_rng_strategy() {return rng_strategy;};
    void set_rng_strategy(std::string strategy) {rng_strategy = strategy;};

    // Function to run a single simulation segment
    void runSimSegment(int repeat, 
                       std::vector<short>& wRaces,
                       std::vector<short>& wSexes,
                       std::vector<short>& wYearBirths,
                       std::vector<short>& initiationAge,
                       std::vector<short>& cessationAge,
                       std::vector<short>& ageAtDeath,
                       std::vector<std::string>& cpdString,
                       int offset);
};
