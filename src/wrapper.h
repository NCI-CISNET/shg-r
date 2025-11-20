#define STRICT_R_HEADERS
#include <Rcpp.h>
#include "smoking_sim.h"
using namespace std;

// Note that the CLI SHG version might specify default paths differently
#define R_DEFAULT_DATA_DIR "./inputs/default/"
#define R_INITIATION_DATA_FILE "lbc_smokehist_initiation.txt"
#define R_CESSATION_DATA_FILE "lbc_smokehist_cessation.txt"
#define R_OTHER_COD_DATA_FILE "lbc_smokehist_oc_mortality.txt"
#define R_CPD_INTENSITY_PROBS "lbc_smokehist_cpdintensityprobs.txt"  
#define R_CPD_DATA_FILE "lbc_smokehist_cpd.txt" 

std::string find_default_data_path() {
    Rcpp::Environment base("package:base");
    Rcpp::Function sys_file = base["system.file"];
    Rcpp::StringVector path = "";
    std::string default_data_path = R_DEFAULT_DATA_DIR;

    // Depending on local testing environment or installed package environment, the path to the default data will vary
    // TODO: review
 
    // Try to find the inst/inputs/default folder; if empty, the folder was not found
    path = sys_file("inst/inputs", "default", Rcpp::_["package"] = "SmokingHistoryGenerator");
    default_data_path = Rcpp::as<std::string>(path);

    if (default_data_path.length() == 0) {
        // inst/inputs/default folder not found so package has likely been installed in R without the inst folder
        path = sys_file("inputs", "default", Rcpp::_["package"] = "SmokingHistoryGenerator");
        default_data_path = Rcpp::as<std::string>(path);
    }
    else if (default_data_path.length() == 0) {
        // If still not found, we use default relative path;
        default_data_path = R_DEFAULT_DATA_DIR;
    }

    return default_data_path;
}

class SHGInterface {
public:
    SHGInterface();
    SHGInterface(Rcpp::List config);
    // Function to run simulations in parallel and combine results
    bool isValidDataFrame(Rcpp::DataFrame& dfPopulation);
    Rcpp::DataFrame runSimFromFixedValues(int repeat, short wRace, short wSex, short wYearBirth);
    Rcpp::DataFrame runSimFromDataFrame(Rcpp::DataFrame dfPopulation);

    string input_data_folder = find_default_data_path();
    string initiation_filename = R_INITIATION_DATA_FILE;
    string cessation_filename = R_CESSATION_DATA_FILE;
    string lifetable_filename = R_OTHER_COD_DATA_FILE;
    string cpd_filename = R_CPD_DATA_FILE;    
    int immediate_cessation_year = 0;

    bool fileExists(const char* filename);
    Smoking_Simulator* loadSimulator();

    int number_of_segments = 1;
    bool run_multi_threaded = false;
    string rng_strategy = "RngStream";
    
    // Seed storage for RNG strategies
    vector<unsigned long> mt_seeds;  // 4 seeds for MersenneTwister (initiation, cessation, life table, individual)
    vector<unsigned long> rngstream_seed;  // 6-element seed array for RngStream

    // Getters and Setters
    int get_number_of_segments() {return number_of_segments;};
    void set_number_of_segments(int n);

    bool get_run_multi_threaded() {return run_multi_threaded;};
    void set_run_multi_threaded(bool b);

    string get_rng_strategy() {return rng_strategy;};
    void set_rng_strategy(string strategy);

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

    Rcpp::NumericVector get_mt_seeds();
    void set_mt_seeds(Rcpp::NumericVector seeds);

    Rcpp::NumericVector get_rngstream_seed();
    void set_rngstream_seed(Rcpp::NumericVector seed);

    Rcpp::NumericVector get_current_seeds();
    void reset_seeds_to_defaults();
    Rcpp::NumericVector get_rng_state_fingerprint();

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

    // Configuration management
    Rcpp::List getConfig(bool debug);
    Rcpp::List getConfig();  // Wrapper without debug parameter (defaults to false)
    void useConfig(Rcpp::List config);

};


