#define STRICT_R_HEADERS
#include <Rcpp.h>
#include "smoking_sim.h"

class SHGInterface {
public:
    SHGInterface();
    Rcpp::DataFrame runSim(int repeat, short wRace, short wSex, short wYearBirth);
    void initialize();
    void setRNGs(SEXP rng1, SEXP rng2, SEXP rng3, SEXP rng4);
    void runSimFromInputFile(const char *sInputFileName);
    void setRNGtype(std::string RNGtype);
    const char *sInputFile;
    const char *sOutputFile;
    Smoking_Simulator *pSimulator = 0;
};
