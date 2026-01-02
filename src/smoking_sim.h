// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: smoking_sim.h
// Author: Martin Krapcho & Ben Racine
// E-Mail: KrapchoM@imsweb.com & ben.racine@cornerstonenw.com
// NCI Contacts: Rocky Feuer

#ifndef _SMOKING_SIM_H
#define _SMOKING_SIM_H

#include "mersenne_class.h"
#include "sim_exception.h"
#include "rng_strategy.h"
#include <string.h>
#include <iostream>
#include <mutex>
#include <atomic>
#include <vector>

#ifdef IS_RCPP
#include <Rcpp.h>
// CRAN-compliant: use REprintf instead of fprintf(stderr, ...)
#define SHG_STDERR(...) REprintf(__VA_ARGS__)
#else
#define SHG_STDERR(...) fprintf(stderr, __VA_ARGS__)
#endif

// Constants used in Excess Risk Former Smokers' formula
#define B0 -0.1711
#define B1 0.00102
#define B2 0.00171
#define B3 1.08

extern short wSIM_CUTOFF_YEAR;
extern const short wMIN_IMMEDIATE_CESSATION_YEAR;
extern const char sSEX_LABELS[2][7];
extern const char sRACE_LABELS[2][10];

// Forward declaration
class Smoking_Simulator;

// Shared data structure for parallel processing
// Allows multiple Smoking_Simulator instances to share the same loaded input data
class SmokingSimulatorSharedData {
   public:
      // Probability Arrays (shared across instances)
      double *gdInitiationProbs;
      double *gdCessationProbs;
      double *gdLifeTableProbs;
      double *gdIntensityProbs;
      long double *gdCigarettesPerDay;
      
      // Data limit variables (shared across instances)
      short gwNumBirthCohorts;
      short *gwYOBCohortStartYrs;
      short *gwYOBCohortEndYrs;
      short gwNumRaceValues;
      short gwNumSexValues;
      short gwMinInitiationAge;
      short gwMinCessationAge;
      short gwMaxInitiationAge;
      short gwMaxCessationAge;
      short gwMinLifeTableAge;
      short gwMaxLifeTableAge;
      short gwMinLifeTableYear;
      short gwMaxLifeTableYear;
      short gwNumIntensityGrps;
      long gwIntensityMinAge;
      long gwIntensityMaxAge;
      long gwCpdMinAge;
      long gwCpdMaxAge;
      
      // CPD loading statistics
      long glCpdRowsLoaded;
      long glCpdRowsSkipped;
      
      // Offset values for Probability Arrays (shared across instances)
      long gwInitProbRaceOffset;
      long gwInitProbSexOffset;
      long gwInitProbYOBOffset;
      long gwCessProbRaceOffset;
      long gwCessProbSexOffset;
      long gwCessProbYOBOffset;
      long glLifeTabAgeOffset;
      long glLifeTabRaceOffset;
      long glLifeTabSexOffset;
      long glLifeTabYOBOffset;
      long gwIntensityAgeOffset;
      long gwIntensitySexOffset;
      long gwIntensityRaceOffset;
      long glCpdAgeOffset;
      long glCpdRaceOffset;
      long glCpdSexOffset;
      long glCpdYOBOffset;
      short gwNumSmokingGrps;
      
      // Reference counter for memory management (atomic for thread safety)
      std::atomic<int> refCount;
      
      SmokingSimulatorSharedData() : 
                     gdInitiationProbs(0), gdCessationProbs(0),
                     gdLifeTableProbs(0), gdIntensityProbs(0), gdCigarettesPerDay(0),
                     gwYOBCohortStartYrs(0), gwYOBCohortEndYrs(0),
                     // Initialize all metadata fields to safe defaults
                     gwNumBirthCohorts(0), gwNumRaceValues(0), gwNumSexValues(0),
                     gwMinInitiationAge(0), gwMinCessationAge(0),
                     gwMaxInitiationAge(0), gwMaxCessationAge(0),
                     gwMinLifeTableAge(0), gwMaxLifeTableAge(0),
                     gwMinLifeTableYear(0), gwMaxLifeTableYear(0),
                     gwNumIntensityGrps(0), gwIntensityMinAge(0), gwIntensityMaxAge(0),
                     gwCpdMinAge(0), gwCpdMaxAge(0),
                     glCpdRowsLoaded(0), glCpdRowsSkipped(0),
                     // Initialize all offset values
                     gwInitProbRaceOffset(0), gwInitProbSexOffset(0), gwInitProbYOBOffset(0),
                     gwCessProbRaceOffset(0), gwCessProbSexOffset(0), gwCessProbYOBOffset(0),
                     glLifeTabAgeOffset(0), glLifeTabRaceOffset(0), glLifeTabSexOffset(0), glLifeTabYOBOffset(0),
                     gwIntensityAgeOffset(0), gwIntensitySexOffset(0), gwIntensityRaceOffset(0),
                     glCpdAgeOffset(0), glCpdRaceOffset(0), glCpdSexOffset(0), glCpdYOBOffset(0),
                     gwNumSmokingGrps(0),
                     refCount(1) {}
      
      void addRef() { refCount.fetch_add(1, std::memory_order_relaxed); }
      void release() {
         if (refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete [] gdInitiationProbs;
            delete [] gdCessationProbs;
            delete [] gdLifeTableProbs;
            delete [] gdIntensityProbs;
            delete [] gdCigarettesPerDay;
            delete [] gwYOBCohortStartYrs;
            delete [] gwYOBCohortEndYrs;
            delete this;
         }
      }
};

class Smoking_Simulator {

   // Labels and Enumerated Data Types for the class
   public:

      enum DataType {DATA_Initiation = 1, DATA_Cessation};
      enum OutputType {OUT_DataOnly = 1, OUT_TextReport, OUT_TimeLine, OUT_XML_Tags, OUT_Uninitialized};

      // Individuals smoking status
      enum SmokingStatus {SMKST_Never = 0, SMKST_Current, SMKST_Former, SMKST_NumValues};

      // Individuals smoking frequency quintile (light to heavy)
      enum SmokingIntensity {SMKR_Light = 0, SMKR_LgtMed, SMKR_Medium, SMKR_MedHvy, SMKR_Heavy, SMKR_NumGroups, SMKR_Uninitialized};

      // Columns of data in the other COD Life Table file
      enum LifeTableColumns {COL_Never = 0, COL_Current_Q1, COL_Current_Q2, COL_Current_Q3, COL_Current_Q4, COL_Current_Q5, COL_NumColumns};

      // These 2 enums are used to write the input tag for the web version output
      enum Sex {SEX_Male = 0, SEX_Female, NUM_SEXES};
      enum Race {RACE_AllRaces = 0, NUM_RACES};


 	// Private Member Variables
   private:
      RNG_Strategy* gpRngStrategy;// Pointer to the RNG strategy (Mersenne Twister by default, RngStream, or a custom strategy)
      static std::mutex dataMutex; // Mutex to protect shared resources

      // Probability Arrays
      double *gdInitiationProbs;  // Prob of initiation by race/sex/year of birth and age
      double *gdCessationProbs;   // Prob of cessation by race/sex/year of birth and age
      double *gdLifeTableProbs;   // Prob of COD other than lung cancer by race/sex/year of birth/age and smoking status
      double *gdIntensityProbs;   // Prob of being a light to heavy smoker (for individuals that begin smoking)

      // Cigarettes per day by race, sex, YOB and age (and smoking intensity? %bjr)
      long double *gdCigarettesPerDay;
      
      // Data limit variables
      short gwNumBirthCohorts;    // Number of birth cohorts Available
      short *gwYOBCohortStartYrs; // Starting year for each of the birth cohort groups
      short *gwYOBCohortEndYrs;   // Ending year for each of the birth cohort groups
      short gwNumRaceValues;      // Number of Races Available
      short gwNumSexValues;       // Number of Sexes Available
      short gwMinInitiationAge;   // Min initiation age (assumed constant for all cohort groups)
      short gwMinCessationAge;    // Min cessation age (assumed constant for all cohort groups)
      short gwMaxInitiationAge;   // Max initiation age (Max possible age, data may quit before max age)
      short gwMaxCessationAge;    // Max cessation age (Max possible age, data may quit before max age)
      short gwMinLifeTableAge;
      short gwMaxLifeTableAge;
      short gwMinLifeTableYear;
      short gwMaxLifeTableYear;
      short gwNumIntensityGrps;    // Number of CPD Intesity groups
      long gwIntensityMinAge;      // Minimum age among the smoking intensity group probabilities
      long gwIntensityMaxAge;      // Maximum age among the smoking intensity group probabilities
      long gwCpdMinAge;            // Minimum age in the cigarettes per day data
      long gwCpdMaxAge;            // Maximum age in the cigarettes per day data
      short gwImmediateCessYear;   // Year when all smokers automatically quit smoking. 0 = option not used.
      bool gbImmediateCessation;   // Is immediatte Cessation turned on

      // Person Variables, Store the results for the last person simulated
      short gwPersonsYOB;          // Year Of Birth
      short gwPersonsRace;         // Race
      short gwPersonsSex;          // Sex
      short gwPersonsInitAge;      // Age of Smoking Initiation
      short gwPersonsCessAge;      // Age of Smoking Cessation
      short gwPersonsAgeAtDeath;   // Age at death from COD other than lung cancer
      SmokingIntensity gwPersonsSmkIntensity; // The smoking intesity group for the person (smokers only)
      double *gdPersonsCPDbyAge;   // Cigarettes smoked per day by age (points to gdPersonsCPDbyAgeStorage)
      double gdPersonsCPDbyAgeStorage[100];  // Pre-allocated storage for CPD (max 100 years)
      double gdPersonsAvgCPD;      // Average num of Cigarettes smoked per day (used for COD in former smokers)

      // Offset values for Probability Arrays
      long gwInitProbRaceOffset;   // Initiation Array - Race Offset
      long gwInitProbSexOffset;    // Initiation Array - Sex Offset
      long gwInitProbYOBOffset;    // Initiation Array - YOB Offset
      long gwCessProbRaceOffset;   // Cessation Array  - Race Offset
      long gwCessProbSexOffset;    // Cessation Array  - Sex Offset
      long gwCessProbYOBOffset;    // Cessation Array  - YOB Offset
      long glLifeTabAgeOffset;     // Other COD Array  - Age Offset
      long glLifeTabRaceOffset;    // Other COD Array  - Race Offset
      long glLifeTabSexOffset;     // Other COD Array  - Sex Offset
      long glLifeTabYOBOffset;     // Other COD Array  - YOB Offset
      long gwIntensityAgeOffset;   // Smoking Intesity Array - Age Offset
      long gwIntensitySexOffset;
      long gwIntensityRaceOffset;
      long glCpdAgeOffset;         // Cigarettes per Day Array  - Age Offset
      long glCpdRaceOffset;        // Cigarettes per Day Array  - Race Offset
      long glCpdSexOffset;         // Cigarettes per Day Array  - Sex Offset
      long glCpdYOBOffset;         // Cigarettes per Day Array  - YOB Offset

      short gwNumSmokingGrps;

      OutputType           geOutputType;

      double      gdTempIntensityProb; // Persons intensity prob, remove from final

      // Shared data management
      SmokingSimulatorSharedData* gpSharedData;  // Pointer to shared data (if using shared data, otherwise NULL)
      bool        gbOwnsData;    // True if this instance owns the data (should delete in Free())

      void Init();
      void Free();
      // void CalcCigarettesPerDay();
      void CalcCigarettesPerDaySwitch();
      short GetAgeOfDeathFromOtherCOD(short wStartAge, short wEndAge, SmokingStatus eStatus, bool &bWentPastData);
      double GetNextInitRand();
      double GetNextCessRand();
      double GetNextLifeTableRand();
      double GetNextRandForIndiv();
      void LoadCPDIntensityProbs(const char* sDataFileName);
      void LoadCPDFile(const char* sCpdDataFile);
      void LoadOtherCODFile(const char* sLifeTableFileName);
      void LoadProbabilityData(const char* sDataFileName, DataType eFileType);
      void OversamplePRNGs();
   
   public:
      bool gbSkipOversampling = false;  // Performance optimization: skip oversampling when not needed
      bool gbSkipValidation = false;    // Performance optimization: skip input validation when inputs are pre-validated
   
   private:
      // Data loading statistics (for warnings/info)
      long glCpdRowsSkipped;            // Count of CPD rows skipped due to cohort mismatch
      long glCpdRowsLoaded;             // Count of CPD rows successfully loaded

   public:

      // Constructor for RunWebVersion (which initiates RNGs after instantiation)
      Smoking_Simulator(const char* sInitiationProbFile,  const char* sCessationProbFile,
                        const char* sLifeTableFile,       const char* sCpdIntensityProbFile,
                        const char* sCpdDataFile,         short wOutputType,
                        short wCessationYear);

      // Constructor with shared data (for parallel processing)
      Smoking_Simulator(SmokingSimulatorSharedData* pSharedData, short wOutputType, short wCessationYear);

      // Constructor
      Smoking_Simulator(const char* sInitiationProbFile, const char* sCessationProbFile,
                        const char* sLifeTableFile,      const char* sCpdIntensityProbFile,
                        const char* sCpdDataFile,        unsigned long ulInitPRNGSeed,
                        unsigned long ulCessPRNGSeed,    unsigned long ulLifeTableSeed,
                        unsigned long ulIndivRndsSeed,   short wOutputType,
                        short wCessationYear);

      ~Smoking_Simulator();

      short GetMaxYearOfBirth();
      short GetMinYearOfBirth();
      short GetNumRaceValues() { return gwNumRaceValues;};
      short GetNumSexValues() { return gwNumSexValues;};
      short GetYOBCohortGroup(short wYearBirth);

      void RunSimulation(const char* sInputFileName, const char* sOutputFileName = 0, bool bPrintToScreen = true);
      void RunSimulationSingle(short wRace, short wSex, short wYearBirth, FILE* pOutStream = 0);

      void SetOutputType(short wOutputType);
      void WriteAsData(FILE *pOutStream);
      void WriteAsText(FILE *pOutStream);
      void WriteAsTimeline(FILE *pOutStream);
      void WriteAsXML(FILE *pOutStream);
      void WriteToStream(FILE *pOutStream);

      // Public getters
      short GetPersonsYOB() {return gwPersonsYOB;}
      short GetPersonsRace() {return gwPersonsRace;}
      short GetPersonsSex() {return gwPersonsSex;}      
      short GetPersonsInitAge() {return gwPersonsInitAge;}
      short GetPersonsCessAge() {return gwPersonsCessAge;}
      short GetPersonsAgeAtDeath() {return gwPersonsAgeAtDeath;}
      SmokingIntensity GetPersonsSmkIntensity() {return gwPersonsSmkIntensity;}
      double* GetPersonsCPDbyAge() {return gdPersonsCPDbyAge;}
      double GetPersonsAvgCPD() {return gdPersonsAvgCPD;}
      void resetRNGStrategy(){
         gpRngStrategy->resetStrategy();
      };
      void setRNGStrategy(RNG_Strategy* rngStrategy);
      void incrementSubstreams(int n) {
         gpRngStrategy->incrementSubstreams(n);
      };
      void writeRNGState() {
         gpRngStrategy->writeRNGState();
      };
      std::vector<double> getRNGStateFingerprint() {
         return gpRngStrategy->getRNGStateFingerprint();
      };

      // Static function to create and load shared data
      static SmokingSimulatorSharedData* CreateSharedData(const char* sInitiationProbFile, const char* sCessationProbFile,
                                          const char* sLifeTableFile, const char* sCpdDataFile);

      // Data shape getters (for info/debugging)
      short GetNumBirthCohorts() { return gwNumBirthCohorts; }
      short GetMinInitiationAge() { return gwMinInitiationAge; }
      short GetMaxInitiationAge() { return gwMaxInitiationAge; }
      short GetMinCessationAge() { return gwMinCessationAge; }
      short GetMaxCessationAge() { return gwMaxCessationAge; }
      long GetCpdMinAge() { return gwCpdMinAge; }
      long GetCpdMaxAge() { return gwCpdMaxAge; }
      short GetNumIntensityGrps() { return gwNumIntensityGrps; }
      long GetCpdRowsSkipped() { return glCpdRowsSkipped; }
      long GetCpdRowsLoaded() { return glCpdRowsLoaded; }
      
      // Get cohort year range for a given cohort index
      short GetCohortStartYear(short cohortIndex) { 
         return (cohortIndex >= 0 && cohortIndex < gwNumBirthCohorts) ? gwYOBCohortStartYrs[cohortIndex] : -1; 
      }
      short GetCohortEndYear(short cohortIndex) { 
         return (cohortIndex >= 0 && cohortIndex < gwNumBirthCohorts) ? gwYOBCohortEndYrs[cohortIndex] : -1; 
      }
      
      // Print data shape summary to stderr
      void PrintDataShapeSummary();

};
// Implemented in main.cpp
int RunWebVersion(const char *sInputFileName);
char* AssignFilename(const char* sDirectory, const char * sFilename);

void WriteToFile(FILE* stream, const char*format, ...);
void PrintMessage(const char* message);
void PrintMessageFormatted(const char* format, ...);
void PrintError(const char* format, ...);

// XML header writing functions (shared between CLI and R wrapper)
// These write the metadata header that CLI produces, allowing R to generate identical output
void WriteRunInfoTag(FILE* pOutStream, const char* sVersion, const char* sInitiationSeed,
                     const char* sCessSeed, const char* sOCDSeed, const char* sMiscSeed,
                     const char* sImmediateCessYear, const char* sInitFile, const char* sCessFile,
                     const char* sOCDProbFile, const char* sQuintilesFile, const char* sCPDDataFile,
                     const char* sOutputFile, const char* sErrorFile, const char* sRNGStrategy, 
                     const char* sRngStreamSeed, const char* sInputFileName,
                     int numSegments, int numThreads, bool multiThreaded, bool autoSegments);

void WriteInputTag(FILE* pOutStream, const char* sRace, const char* sSex, 
                   const char* sYearOfBirth, const char* sNumReps, bool withHoldTags);

void WriteSimulationOpenTag(FILE* pOutStream, bool withHoldTags);
void WriteSimulationCloseTag(FILE* pOutStream, bool withHoldTags);

#endif

