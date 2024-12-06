#define STRICT_R_HEADERS
#include <Rcpp.h>
#include "smoking_sim.h"
using namespace std;

// Note that the CLI SHG version might specify default paths differently
#define R_DEFAULT_DATA_DIR "./inst/inputs/default/"
#define R_INITIATION_DATA_FILE "lbc_smokehist_initiation.txt"
#define R_CESSATION_DATA_FILE "lbc_smokehist_cessation.txt"
#define R_OTHER_COD_DATA_FILE "lbc_smokehist_oc_mortality.txt"
#define R_CPD_INTENSITY_PROBS "lbc_smokehist_cpdintensityprobs.txt"  
#define R_CPD_DATA_FILE "lbc_smokehist_cpd.txt" 

// TODO DRY violation -- but relevant to the CLI version
// #define MAX_NUM_REPS 1000000

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

    string get_rng_strategy() {return rng_strategy;};
    void set_rng_strategy(string strategy) {rng_strategy = strategy;};

    string get_input_data_folder() {return input_data_folder;};
    void set_input_data_folder(string folder) {input_data_folder = folder;};

    string get_initiation_filename() {return initiation_filename;};
    void set_initiation_filename(string filename) { initiation_filename = filename;};

    string get_cessation_filename() {return cessation_filename;};
    void set_cessation_filename(string filename) {cessation_filename = filename;};

    string get_lifetable_filename() {return lifetable_filename;};
    void set_lifetable_filename(string filename) {lifetable_filename = filename;};

    string get_cpd_filename() { return cpd_filename;};
    void set_cpd_filename(string filename) {cpd_filename = filename;};

    int get_immediate_cessation_year() {return immediate_cessation_year;};
    void set_immediate_cessation_year(int n) {immediate_cessation_year = n;};

    // Function to run a single simulation segment
    void runSimSegment(int repeat, 
                       vector<short>& wRaces,
                       vector<short>& wSexes,
                       vector<short>& wYearBirths,
                       vector<short>& initiationAge,
                       vector<short>& cessationAge,
                       vector<short>& ageAtDeath,
                       vector<string>& cpdString,
                       int offset);
};
