#define STRICT_R_HEADERS
#include <Rcpp.h>
#include "smoking_sim.h"

class SHGInterface {
public:
    SHGInterface();
    // Function to run simulations in parallel and combine results
    Rcpp::DataFrame runSim(int repeat, short wRace, short wSex, short wYearBirth);

    void initialize();
    void LegacyRunWebVersion(const char *sInputFileName);
    const char *sInputFile;
    const char *sOutputFile;
    Smoking_Simulator *pSimulator = 0;
    Smoking_Simulator* createSimulator();

    // Function to run a single simulation segment
    void runSimSegment(int repeat, short wRace, short wSex, short wYearBirth,
                       std::vector<int>& initiationAge, std::vector<int>& cessationAge,
                       std::vector<int>& ageAtDeath, std::vector<std::string>& cpdString,
                       int offset);
};
