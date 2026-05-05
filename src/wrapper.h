#define STRICT_R_HEADERS
#include <Rcpp.h>
#include "smoking_sim.h"
using namespace std;

// Note that the CLI SHG version might specify default paths differently
#define R_DEFAULT_DATA_DIR "./extdata/"
#define R_INITIATION_DATA_FILE "initiation.csv"
#define R_CESSATION_DATA_FILE "cessation.csv"
#define R_OTHER_COD_DATA_FILE "ocm-excl-lung-cancer.csv"
#define R_CPD_DATA_FILE "cpd.csv"

std::string find_default_data_path() {
    Rcpp::Environment base("package:base");
    Rcpp::Function sys_file = base["system.file"];
    Rcpp::StringVector path = "";
    std::string default_data_path = R_DEFAULT_DATA_DIR;

    // Depending on local testing environment or installed package environment, the path to the default data will vary
    // TODO: review
 
    // Installed layout: inst/extdata/* -> .../extdata/
    path = sys_file("extdata", Rcpp::_["package"] = "SmokingHistoryGenerator");
    default_data_path = Rcpp::as<std::string>(path);

    if (default_data_path.length() == 0) {
        // pkgload::load_all() / devtools sometimes resolves inst/ before install
        path = sys_file("inst", "extdata", Rcpp::_["package"] = "SmokingHistoryGenerator");
        default_data_path = Rcpp::as<std::string>(path);
    }
    if (default_data_path.length() == 0) {
        // Older installs may still have inputs/default
        path = sys_file("inputs", "default", Rcpp::_["package"] = "SmokingHistoryGenerator");
        default_data_path = Rcpp::as<std::string>(path);
    }
    if (default_data_path.length() == 0) {
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
    string mortality_filename_ = R_OTHER_COD_DATA_FILE;
    string cpd_filename = R_CPD_DATA_FILE;
    /** Last load_params() source URL or local zip path (R package only; empty if unset). */
    string params_bundle_source_ = "";
    /** Last load_params() mortality choice: "acm" or "ocm" (empty if unset). */
    string params_mortality_ = "";
    int immediate_cessation_year = 0;

    bool fileExists(const char* filename);
    Smoking_Simulator* loadSimulator();
    void setupRNGStrategy(Smoking_Simulator* qSimulator);  // Helper to avoid DRY

    int number_of_segments = -1;  // -1 = auto-calculate when multi-threaded
    int num_threads = -1;         // -1 = auto (all cores), 1 = single-threaded, N = N threads
    string rng_strategy = "RngStream";
    string cpd_format = "sparse"; // "none" (fastest), "sparse" (default), "legacy" (backwards compat)
    string output_file = "";      // Empty = return DataFrame; set path = write to disk like CLI
    
    // Seed storage for RNG strategies (store MT seeds as double for exact R round-trip on all platforms).
    vector<double> mt_seeds;  // 4 seeds for MersenneTwister (initiation, cessation, life table, individual)
    vector<unsigned long> rngstream_seed;  // 6-element seed array for RngStream
    // Effective runtime settings captured from the most recent simulation call.
    bool has_effective_runtime_config_ = false;
    int last_effective_number_of_segments_ = -1;
    int last_effective_num_threads_ = -1;
    // Last inferred single-cohort simulation context (if available).
    bool has_last_cohort_year_ = false;
    int last_cohort_year_ = 0;
    // Last runSimFromFixedValues request (if available).
    bool has_last_fixed_run_ = false;
    int last_fixed_repeat_ = 0;
    short last_fixed_race_ = 0;
    short last_fixed_sex_ = 0;
    /** runSimFromFixedValues sets this true immediately before delegating to runSimFromDataFrame. */
    bool next_dataframe_call_is_fixed_cohort_ = false;
    bool last_completed_sim_was_fixed_cohort_ = false;

    bool last_completed_sim_was_fixed_cohort() const { return last_completed_sim_was_fixed_cohort_; }

    // Getters and Setters
    int get_number_of_segments() {return number_of_segments;};
    void set_number_of_segments(int n);

    int get_num_threads() {return num_threads;};
    void set_num_threads(int n);

    string get_rng_strategy() {return rng_strategy;};
    void set_rng_strategy(string strategy);

    string get_cpd_format() {return cpd_format;};
    void set_cpd_format(string format);
    
    string get_output_file() {return output_file;};
    void set_output_file(string path) {output_file = path;};

    string get_input_data_folder() {return input_data_folder;};
    void set_input_data_folder(string folder) {input_data_folder = folder;};

    string get_initiation_filename() {return initiation_filename;};
    void set_initiation_filename(string filename) { initiation_filename = filename;};

    string get_cessation_filename() {return cessation_filename;};
    void set_cessation_filename(string filename) {cessation_filename = filename;};

    string get_mortality_filename() {return mortality_filename_;};
    void set_mortality_filename(string filename) {mortality_filename_ = filename;};

    string get_params_bundle_source() { return params_bundle_source_; };
    void set_params_bundle_source(string s) { params_bundle_source_ = s; };
    string get_params_mortality() { return params_mortality_; };
    void set_params_mortality(string s) { params_mortality_ = s; };

    /** Root directory for load_params() zip cache (R tools::R_user_dir); read-only from R. */
    std::string get_params_cache_dir();

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
    
    // Data shape info (populated after data is loaded)
    Rcpp::List get_data_shape();
    
    // Data shape storage (updated when simulator loads data)
    int last_num_races = 0;
    int last_num_sexes = 0;
    int last_num_cohorts = 0;
    int last_min_init_age = 0;
    int last_max_init_age = 0;
    int last_min_cess_age = 0;
    int last_max_cess_age = 0;
    long last_cpd_min_age = 0;
    long last_cpd_max_age = 0;
    int last_num_intensity_grps = 0;
    long last_cpd_rows_loaded = 0;
    long last_cpd_rows_skipped = 0;
    int last_first_cohort_start = 0;
    int last_first_cohort_end = 0;
    int last_last_cohort_start = 0;
    int last_last_cohort_end = 0;

    void runSimSegment(int repeat, 
                       vector<short>& wRaces,
                       vector<short>& wSexes,
                       vector<short>& wYearBirths,
                       vector<short>& initiationAge,
                       vector<short>& cessationAge,
                       vector<short>& ageAtDeath,
                       vector<string>& cpdString,
                       int offset,
                       SmokingSimulatorSharedData* pSharedData);


    // File output mode - writes directly to disk like CLI (DRY: reuses WriteAsData)
    void runSimSegmentToFile(int repeat,
                             vector<short>& wRaces,
                             vector<short>& wSexes,
                             vector<short>& wYearBirths,
                             int offset,
                             const string& tempFilePath,
                             SmokingSimulatorSharedData* pSharedData,
                             int segmentNumber);
    void assembleSegmentFiles(const vector<string>& tempFiles, const string& outputFile,
                               int repeat, int race, int sex, int yob,
                               int effectiveSegments, bool bMultiThreaded, bool bAutoSegments);

    void LegacyRunWebVersion(const char *sInputFileName);

    // Configuration management
    Rcpp::List getConfig(bool debug);
    Rcpp::List getConfig();  // Wrapper without debug parameter (defaults to false)
    void useConfig(Rcpp::List config);

};


