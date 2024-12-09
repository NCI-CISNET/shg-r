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

    string input_data_folder = R_DEFAULT_DATA_DIR;
    string initiation_filename = R_INITIATION_DATA_FILE;
    string cessation_filename = R_CESSATION_DATA_FILE;
    string lifetable_filename = R_OTHER_COD_DATA_FILE;
    string cpd_filename = R_CPD_DATA_FILE;    
    int immediate_cessation_year = 0;

    bool fileExists(const char* filename);
    Smoking_Simulator* loadSimulator();

    int number_of_segments = 10;
    bool run_multi_threaded = true;
    string rng_strategy = "RngStream";

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

    void runSimSegment(int repeat, 
                       vector<short>& wRaces,
                       vector<short>& wSexes,
                       vector<short>& wYearBirths,
                       vector<short>& initiationAge,
                       vector<short>& cessationAge,
                       vector<short>& ageAtDeath,
                       vector<string>& cpdString,
                       int offset);

    void LegacyRunWebVersion(const char *sInputFileName);

};
