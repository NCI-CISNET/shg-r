// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: smoking_sim.cpp
// Author: Martin Krapcho & Ben Racine
// E-Mail: KrapchoM@imsweb.com & ben.racine@cornerstonenw.com
// NCI Contact: Rocky Feuer

#ifdef IS_R
#include <Rcpp.h>
// [[Rcpp::depends(Rcpp)]]
#endif

#include "smoking_sim.h"
#include <string>
#include <limits>
#include <climits>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <cstdarg>
#include <cstring>  // for memset
#include <mutex>
#include <vector>
#include "rng_strategy.h"
using namespace std;

// Fast integer to string - returns length written
inline int fast_itoa(int val, char* buf) {
   char* start = buf;
   bool neg = val < 0;
   if (neg) { *buf++ = '-'; val = -val; }
   if (val == 0) { *buf++ = '0'; }
   else {
      char tmp[12]; int i = 0;
      while (val) { tmp[i++] = '0' + (val % 10); val /= 10; }
      while (i--) *buf++ = tmp[i];
   }
   return (int)(buf - start);
}

// Fast double to string with 2 decimal places - returns length written
inline int fast_dtoa2(double val, char* buf) {
   int ival = (int)(val * 100 + 0.5);
   int frac = ival % 100;
   ival /= 100;
   char* start = buf;
   if (ival == 0) { *buf++ = '0'; }
   else {
      char tmp[12]; int i = 0;
      while (ival) { tmp[i++] = '0' + (ival % 10); ival /= 10; }
      while (i--) *buf++ = tmp[i];
   }
   *buf++ = '.';
   *buf++ = '0' + (frac / 10);
   *buf++ = '0' + (frac % 10);
   return (int)(buf - start);
}

// Constructor for RunWebVersion call in main (removed the PRNG seeds as they are set in the RNG strategy)
Smoking_Simulator::Smoking_Simulator(const char* sInitiationProbFile, const char* sCessationProbFile,
                                     const char* sMortalityFile,      const char* sCpdIntensityProbFile,
                                     const char* sCpdDataFile,        short wOutputType,
                                     short wCessationYear) { 

   char sErrorMessage[300];
   try {
      Init();
      // TODO: performance improvement possible: only load data once rather than for each segment
      // Loading costs about 0.03 seconds (so 0.3 seconds for 10 segments). Estimated 10% time improvement for 1M runs if we can load once.
      LoadProbabilityData(sInitiationProbFile, Smoking_Simulator::DATA_Initiation);
      LoadProbabilityData(sCessationProbFile, Smoking_Simulator::DATA_Cessation);
      LoadCPDFile(sCpdDataFile);
      LoadMortalityFile(sMortalityFile);

      SetOutputType(wOutputType);

      // Immediate Cessation Values are initialized to 0 and false respectively, 
      // Check to see if they need to be changed.
      if ((wCessationYear != 0) || (wCessationYear >= wMIN_IMMEDIATE_CESSATION_YEAR && wCessationYear <= wSIM_CUTOFF_YEAR)) {
         gwImmediateCessYear = wCessationYear;
         gbImmediateCessation = true;
      } else if ( wCessationYear != 0) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Value for Immediate Cessation Year.\n \
            Valid values are 0 and the range %d to %d.\n", wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
         throw SimException("Error", sErrorMessage);
      }
    } catch (SimException ex) {
      ex.AddCallPath("Smoking_Simulator()");
      Free();
      throw ex;
    }
}
// Legacy Constructor
Smoking_Simulator::Smoking_Simulator(const char* sInitiationProbFile, const char* sCessationProbFile,
                                     const char* sMortalityFile,      const char* sCpdIntensityProbFile,
                                     const char* sCpdDataFile,        unsigned long ulInitPRNGSeed,
                                     unsigned long ulCessPRNGSeed,    unsigned long ulMortalitySeed,
                                     unsigned long ulIndivRndsSeed,   short wOutputType,
                                     short wCessationYear) { 

   char sErrorMessage[300];
   try {
      Init();
      LoadProbabilityData(sInitiationProbFile, Smoking_Simulator::DATA_Initiation);
      LoadProbabilityData(sCessationProbFile, Smoking_Simulator::DATA_Cessation);
      LoadCPDFile(sCpdDataFile);
      LoadMortalityFile(sMortalityFile);
  
      // To maintain the legacy constructor signature, we need to initialize default Mersenne Twister strategy here
      MersenneTwisterRNG* pRngStrategy = new MersenneTwisterRNG(ulInitPRNGSeed, ulCessPRNGSeed, ulMortalitySeed, ulIndivRndsSeed);
      setRNGStrategy(pRngStrategy);
      SetOutputType(wOutputType);

      // Immediate Cessation Values are initialized to 0 and false respectively, 
      // Check to see if they need to be changed.
      if ((wCessationYear != 0) || (wCessationYear >= wMIN_IMMEDIATE_CESSATION_YEAR && wCessationYear <= wSIM_CUTOFF_YEAR)) {
         gwImmediateCessYear = wCessationYear;
         gbImmediateCessation = true;
      } else if ( wCessationYear != 0) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Value for Immediate Cessation Year.\n \
            Valid values are 0 and the range %d to %d.\n", wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
         throw SimException("Error", sErrorMessage);
      }
    } catch (SimException ex) {
      ex.AddCallPath("Smoking_Simulator()");
      Free();
      throw ex;
    }
}

// Constructor with shared data (for parallel processing)
Smoking_Simulator::Smoking_Simulator(SmokingSimulatorSharedData* pSharedData, short wOutputType, short wCessationYear) {
   char sErrorMessage[300];
   try {
      Init();
      
      // Use shared data instead of loading our own
      gpSharedData = pSharedData;
      gpSharedData->addRef();  // Increment reference count
      gbOwnsData = false;
      
      // Copy pointers from shared data to instance variables
      gdInitiationProbs = gpSharedData->gdInitiationProbs;
      gdCessationProbs = gpSharedData->gdCessationProbs;
      gdMortalityProbs = gpSharedData->gdMortalityProbs;
      gdIntensityProbs = gpSharedData->gdIntensityProbs;
      gdCigarettesPerDay = gpSharedData->gdCigarettesPerDay;
      gwYOBCohortStartYrs = gpSharedData->gwYOBCohortStartYrs;
      gwYOBCohortEndYrs = gpSharedData->gwYOBCohortEndYrs;
      
      // Copy metadata from shared data
      gwNumBirthCohorts = gpSharedData->gwNumBirthCohorts;
      gwNumRaceValues = gpSharedData->gwNumRaceValues;
      gwNumSexValues = gpSharedData->gwNumSexValues;
      gwMinInitiationAge = gpSharedData->gwMinInitiationAge;
      gwMinCessationAge = gpSharedData->gwMinCessationAge;
      gwMaxInitiationAge = gpSharedData->gwMaxInitiationAge;
      gwMaxCessationAge = gpSharedData->gwMaxCessationAge;
      gwMinMortalityAge = gpSharedData->gwMinMortalityAge;
      gwMaxMortalityAge = gpSharedData->gwMaxMortalityAge;
      gwMinMortalityYear = gpSharedData->gwMinMortalityYear;
      gwMaxMortalityYear = gpSharedData->gwMaxMortalityYear;
      gwNumIntensityGrps = gpSharedData->gwNumIntensityGrps;
      gwIntensityMinAge = gpSharedData->gwIntensityMinAge;
      gwIntensityMaxAge = gpSharedData->gwIntensityMaxAge;
      gwCpdMinAge = gpSharedData->gwCpdMinAge;
      gwCpdMaxAge = gpSharedData->gwCpdMaxAge;
      
      // Copy offsets
      gwInitProbRaceOffset = gpSharedData->gwInitProbRaceOffset;
      gwInitProbSexOffset = gpSharedData->gwInitProbSexOffset;
      gwInitProbYOBOffset = gpSharedData->gwInitProbYOBOffset;
      gwCessProbRaceOffset = gpSharedData->gwCessProbRaceOffset;
      gwCessProbSexOffset = gpSharedData->gwCessProbSexOffset;
      gwCessProbYOBOffset = gpSharedData->gwCessProbYOBOffset;
      glMortTabAgeOffset = gpSharedData->glMortTabAgeOffset;
      glMortTabRaceOffset = gpSharedData->glMortTabRaceOffset;
      glMortTabSexOffset = gpSharedData->glMortTabSexOffset;
      glMortTabYOBOffset = gpSharedData->glMortTabYOBOffset;
      gwIntensityAgeOffset = gpSharedData->gwIntensityAgeOffset;
      gwIntensitySexOffset = gpSharedData->gwIntensitySexOffset;
      gwIntensityRaceOffset = gpSharedData->gwIntensityRaceOffset;
      glCpdAgeOffset = gpSharedData->glCpdAgeOffset;
      glCpdRaceOffset = gpSharedData->glCpdRaceOffset;
      glCpdSexOffset = gpSharedData->glCpdSexOffset;
      glCpdYOBOffset = gpSharedData->glCpdYOBOffset;
      gwNumSmokingGrps = gpSharedData->gwNumSmokingGrps;
      
      SetOutputType(wOutputType);
      
      // Immediate Cessation Values
      if ((wCessationYear != 0) || (wCessationYear >= wMIN_IMMEDIATE_CESSATION_YEAR && wCessationYear <= wSIM_CUTOFF_YEAR)) {
         gwImmediateCessYear = wCessationYear;
         gbImmediateCessation = true;
      } else if (wCessationYear != 0) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Value for Immediate Cessation Year.\n \
            Valid values are 0 and the range %d to %d.\n", wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
         throw SimException("Error", sErrorMessage);
      }
   } catch (SimException ex) {
      ex.AddCallPath("Smoking_Simulator(SharedData*, short, short)");
      Free();
      throw ex;
   }
}

// Destructor
Smoking_Simulator::~Smoking_Simulator() {
   Free();
}

// Static function to create and load shared data for parallel processing
// This loads the input files once and returns a SharedData structure that can be
// used by multiple Smoking_Simulator instances
SmokingSimulatorSharedData* Smoking_Simulator::CreateSharedData(
      const char* sInitiationProbFile, const char* sCessationProbFile,
      const char* sMortalityFile, const char* sCpdDataFile) {
   
   // Create a temporary simulator to load the data using existing loading code
   short wOutputType = OUT_DataOnly;
   short wCessationYear = 0;
   const char* sCpdIntensityFile = ""; // no longer used
   
   Smoking_Simulator* pTempSim = new Smoking_Simulator(
      sInitiationProbFile, sCessationProbFile, sMortalityFile, 
      sCpdIntensityFile, sCpdDataFile, wOutputType, wCessationYear);
   
   // Create SharedData and transfer ownership of data arrays
   SmokingSimulatorSharedData* pSharedData = new SmokingSimulatorSharedData();
   
   // Transfer data pointers
   pSharedData->gdInitiationProbs = pTempSim->gdInitiationProbs;
   pSharedData->gdCessationProbs = pTempSim->gdCessationProbs;
   pSharedData->gdMortalityProbs = pTempSim->gdMortalityProbs;
   pSharedData->gdIntensityProbs = pTempSim->gdIntensityProbs;
   pSharedData->gdCigarettesPerDay = pTempSim->gdCigarettesPerDay;
   pSharedData->gwYOBCohortStartYrs = pTempSim->gwYOBCohortStartYrs;
   pSharedData->gwYOBCohortEndYrs = pTempSim->gwYOBCohortEndYrs;
   
   // Transfer metadata
   pSharedData->gwNumBirthCohorts = pTempSim->gwNumBirthCohorts;
   pSharedData->gwNumRaceValues = pTempSim->gwNumRaceValues;
   pSharedData->gwNumSexValues = pTempSim->gwNumSexValues;
   pSharedData->gwMinInitiationAge = pTempSim->gwMinInitiationAge;
   pSharedData->gwMinCessationAge = pTempSim->gwMinCessationAge;
   pSharedData->gwMaxInitiationAge = pTempSim->gwMaxInitiationAge;
   pSharedData->gwMaxCessationAge = pTempSim->gwMaxCessationAge;
   pSharedData->gwMinMortalityAge = pTempSim->gwMinMortalityAge;
   pSharedData->gwMaxMortalityAge = pTempSim->gwMaxMortalityAge;
   pSharedData->gwMinMortalityYear = pTempSim->gwMinMortalityYear;
   pSharedData->gwMaxMortalityYear = pTempSim->gwMaxMortalityYear;
   pSharedData->gwNumIntensityGrps = pTempSim->gwNumIntensityGrps;
   pSharedData->gwIntensityMinAge = pTempSim->gwIntensityMinAge;
   pSharedData->gwIntensityMaxAge = pTempSim->gwIntensityMaxAge;
   pSharedData->gwCpdMinAge = pTempSim->gwCpdMinAge;
   pSharedData->gwCpdMaxAge = pTempSim->gwCpdMaxAge;
   pSharedData->glCpdRowsLoaded = pTempSim->glCpdRowsLoaded;
   pSharedData->glCpdRowsSkipped = pTempSim->glCpdRowsSkipped;
   
   // Transfer offsets
   pSharedData->gwInitProbRaceOffset = pTempSim->gwInitProbRaceOffset;
   pSharedData->gwInitProbSexOffset = pTempSim->gwInitProbSexOffset;
   pSharedData->gwInitProbYOBOffset = pTempSim->gwInitProbYOBOffset;
   pSharedData->gwCessProbRaceOffset = pTempSim->gwCessProbRaceOffset;
   pSharedData->gwCessProbSexOffset = pTempSim->gwCessProbSexOffset;
   pSharedData->gwCessProbYOBOffset = pTempSim->gwCessProbYOBOffset;
   pSharedData->glMortTabAgeOffset = pTempSim->glMortTabAgeOffset;
   pSharedData->glMortTabRaceOffset = pTempSim->glMortTabRaceOffset;
   pSharedData->glMortTabSexOffset = pTempSim->glMortTabSexOffset;
   pSharedData->glMortTabYOBOffset = pTempSim->glMortTabYOBOffset;
   pSharedData->gwIntensityAgeOffset = pTempSim->gwIntensityAgeOffset;
   pSharedData->gwIntensitySexOffset = pTempSim->gwIntensitySexOffset;
   pSharedData->gwIntensityRaceOffset = pTempSim->gwIntensityRaceOffset;
   pSharedData->glCpdAgeOffset = pTempSim->glCpdAgeOffset;
   pSharedData->glCpdRaceOffset = pTempSim->glCpdRaceOffset;
   pSharedData->glCpdSexOffset = pTempSim->glCpdSexOffset;
   pSharedData->glCpdYOBOffset = pTempSim->glCpdYOBOffset;
   pSharedData->gwNumSmokingGrps = pTempSim->gwNumSmokingGrps;
   
   // Mark the temporary simulator as not owning the data so it won't free it
   pTempSim->gbOwnsData = false;
   
   // Delete the temporary simulator (data arrays will NOT be freed due to gbOwnsData=false)
   delete pTempSim;
   
   return pSharedData;
}

// Calculate the number of cigarettes smoked per day for people that initiate smoking.
// This function first categorizes the person into one of five intensity groups (light to heavy smokers)
// The intensity groups comes from a probability by age lookup table, a random number is given to the individual and then 
// looked up based on initiation age
// If the person begins smoking before age 30
//   - The number of cigarettes per day comes from an uptake formula that calculates the cigarettes smoked
//     per day for the ages less than 30. Details concerning the formula can be obtained from
//     the file provided by Christy Anderson
// The cigarettes smoked per day for ages 30+ come directly from the gdCigarettesPerDay array.
// void Smoking_Simulator::CalcCigarettesPerDay() {

//    short    wIntensityLookupAge,  // Age to look up in the smoking intensity groups
//             wIntensityIndex,      // Index to start at for look up of the smoking intensity groups
//             wYearsAsSmoker,       // Number of years in which person was a smoker.
//             wStartAgeInCpdData,   // The first age that has Cigarettes per day data in the table for the person's year of birth
//             wEndLoop,             // Age at which to end the uptake formula calculation
//             wLookupStartAge,      // Age to start at when getting the cigarettes per day directly from the data array
//             wPersonsYOB,          // Copy of gwPersonsYOB, when the year of birth is less than 1900, 1900 is used in the equation
//             i;
//    long     lCpdStartIndex,       // Index to start at for look up of cigarettes per day
//             lCurrCpdIndex;        // Current index in cigarettes per day array
//    double   dIntensityProb,       // Probability to find in the lookup tables
//             dUptake,              // Uptake formula results for persons current age
//             dUptakeAtCpdStart,    // Uptake formula results at age 30
//             dScalingFactor,       // (Cigarettes per day at age 30)/(Uptake formula at age 30)
//             dSumOfCpd = 0;        // Sum of the annual cigarettes per day value (used to get average)
//    bool     bValueFound;

//    try {

//       if (gdCigarettesPerDay == 0 || gdIntensityProbs == 0 ) { // || gpIndividualRNG == 0) {
//          throw SimException("Error", "One or more of the data components for cigarettes \nper \
//             day calculation has not been initialized.\n");
//       }
//       if (gwPersonsInitAge == -999) {
//          throw SimException("Error", "CalcCigarettesPerDay should not be called for \nindividuals \
//             that do not initiate smoking.\n");
//       }

//       // Get the probability for the quintile lookup
//       dIntensityProb = GetNextRandForIndiv();

//       // Get the age for intensity probabilities lookup
//       // Initiation Ages below the min age use the min intensity age probabilities
//       if (gwPersonsInitAge < gwIntensityMinAge) {      
//          wIntensityLookupAge = gwIntensityMinAge;
//       // Initiation Ages above the max age use the max intensity age probabilities
//       } else if (gwPersonsInitAge > gwIntensityMaxAge) {
//          wIntensityLookupAge = gwIntensityMaxAge;
//       // Otherwise look up the initiation age
//       } else {
//          wIntensityLookupAge = gwPersonsInitAge;
//       }

//       // Set the starting point for the lookup (Age - min age) * age offset
//       bValueFound = false;
//       wIntensityIndex = (wIntensityLookupAge - gwIntensityMinAge) * gwIntensityAgeOffset;


//       // Loop through Intensity probabilities to find quintile for person
//       for (i = 0; i < (gwNumIntensityGrps - 1) && !bValueFound; i++) {
//          if (dIntensityProb <  gdIntensityProbs[i + wIntensityIndex]) {
//             gwPersonsSmkIntensity = (SmokingIntensity) i;
//             bValueFound = true;
//          }
//       }
//       // If the value was not found, assume that the probabilties did not correctly sum to one, 
//       // and assign the person to the last quintile
//       if (!bValueFound) {
//          gwPersonsSmkIntensity = (SmokingIntensity)(SMKR_NumGroups - 1);
//       }

//       gdTempIntensityProb = dIntensityProb;

//       // Set up the array for storing the number of cigarettes smoked per day by age
//       if (gwPersonsCessAge == -999) { 
//          // Person does not quit smoking
//          wYearsAsSmoker = (wSIM_CUTOFF_YEAR - (gwPersonsYOB + gwPersonsInitAge)) + 1;
//       } else {
//          // Person will quit at some time
//          wYearsAsSmoker = (gwPersonsCessAge - gwPersonsInitAge) + 1;
//       }
//       gdPersonsCPDbyAge = new double[wYearsAsSmoker];
//       for ( i = 0; i < wYearsAsSmoker; i++) {
//          gdPersonsCPDbyAge[i] = 0;
//       }

//       // Find the age at which the cigarette per day numbers begin for the persons YOB
//       // In most cases this is age 30, but for those born in 1975-1979 or 1980-1984, the ages are lower (26 and 21)
//       bValueFound      = false;
//       lCpdStartIndex   = (glCpdRaceOffset * (gwPersonsRace)) +
//                          (glCpdSexOffset  * (gwPersonsSex))  +
//                          (glCpdYOBOffset  * GetYOBCohortGroup(gwPersonsYOB)) +
//                          (long)gwPersonsSmkIntensity;
//       lCurrCpdIndex    = lCpdStartIndex;

//       while (!bValueFound) {
//          if (gdCigarettesPerDay[lCurrCpdIndex] >= 0) {
//             bValueFound = true;
//             wStartAgeInCpdData = (short)(((lCurrCpdIndex - lCpdStartIndex) / glCpdAgeOffset) + gwCpdMinAge);
//             lCpdStartIndex = lCurrCpdIndex;
//          } else {
//             lCurrCpdIndex += glCpdAgeOffset;
//          }
//       }

//       // Use the uptake formula to calculate the cigarettes per day before age 30
//       // The age is lower (26 and 21) for the later birth cohorts (1975-1979 and 1980-1984)
//       // In the notes below only age 30 will be referenced but it applies to ages 26 & 21 when necessary
//       if (gwPersonsInitAge < wStartAgeInCpdData) {

//          if ( gwPersonsYOB >= 1900) {
//             wPersonsYOB = gwPersonsYOB;
//          } else {
//             wPersonsYOB = 1900;
//          }

//          // Get age at which to stop the uptake loop
//          wEndLoop = min(wStartAgeInCpdData, (short)(gwPersonsInitAge + wYearsAsSmoker));

//          // Calculate the uptake formulas value at the age where the cigarette per day numbers begin
//          // TODO: should perhaps initialize dUptakeAtCpdStart to something to avoid uninitialized variable warning
//          if (gwPersonsSex == SEX_Male) {
//             dUptakeAtCpdStart = -38.578 + (3.342 * (sqrt(wStartAgeInCpdData - gwPersonsInitAge))) -
//                                 (0.00168 * pow(max(79, ((wPersonsYOB + wStartAgeInCpdData) - 1900 )), 2)) -
//                                 (17.538 * sqrt(wStartAgeInCpdData)) + (44.967 * log(wStartAgeInCpdData));
//          } else if (gwPersonsSex == SEX_Female) {
//             dUptakeAtCpdStart = -56.751 + (0.700*(wStartAgeInCpdData - gwPersonsInitAge)) -
//                                 (0.00163 * pow(max(79, ((wPersonsYOB + wStartAgeInCpdData) - 1900)), 2)) -
//                                 (3.473 * wStartAgeInCpdData) + (32.800 * sqrt(wStartAgeInCpdData));
//          }
//          else {
//             throw SimException("Error", "Invalid value for gwPersonsSex");
//          }

//          // Calculate the Quintile Scaling factor as (cigarettes per day at age 30)/(Uptake at age 30)
//          dScalingFactor = gdCigarettesPerDay[lCpdStartIndex] / dUptakeAtCpdStart;

//          for (i = gwPersonsInitAge; i < wEndLoop; i++) {

//             if (gwPersonsSex == SEX_Male) {
//                dUptake = -38.578 + (3.342 * (sqrt(i - gwPersonsInitAge))) -
//                          (0.00168 * pow(max(79, ((wPersonsYOB + i) - 1900)), 2)) -
//                          (17.538 * sqrt(i)) + (44.967 * log(i));

//             } else if (gwPersonsSex == SEX_Female) {
//                dUptake = -56.751 + (0.700 * (i - gwPersonsInitAge)) -
//                          (0.00163 * pow(max(79, ((wPersonsYOB + i) - 1900)), 2)) -
//                          (3.473 * i) + (32.800 * sqrt(i));
//             }

//             // In the younger ages, the uptake formula might return a negative value, use 0.10 in these cases
//             if (dUptake < 0) {
//                dUptake = 0.10;
//             }

//             gdPersonsCPDbyAge[i - gwPersonsInitAge] = dScalingFactor * dUptake;
//             dSumOfCpd += gdPersonsCPDbyAge[i - gwPersonsInitAge];
//          }
//       }

//       // If the persons started smoking before age 30, fill in the cig per day for ages 30+ (if they didn't quit before 30
//       // Other wise if they started smoking after age 30, fill in the cigarettes per day array starting at that age.
//       if (gwPersonsInitAge <= wStartAgeInCpdData) {
//          wLookupStartAge = wStartAgeInCpdData;
//       } else {
//          wLookupStartAge = gwPersonsInitAge;
//       }

//       // Fill in the Cigarettes per day for ages 30+ directly from the cpd table
//       for ( i = wLookupStartAge; i < (gwPersonsInitAge + wYearsAsSmoker); i++ ) {
//          lCurrCpdIndex = lCpdStartIndex + ((i - wStartAgeInCpdData)*glCpdAgeOffset);
//          if (gdCigarettesPerDay[lCurrCpdIndex] >= 0) {
//             gdPersonsCPDbyAge[i - gwPersonsInitAge] = gdCigarettesPerDay[lCurrCpdIndex];
//          } else {
//             //This is in case the persons age goes past the max cpd for the birth cohort
//             gdPersonsCPDbyAge[i - gwPersonsInitAge] = gdPersonsCPDbyAge[(i - 1) - gwPersonsInitAge];
//          }
//          dSumOfCpd += gdPersonsCPDbyAge[i - gwPersonsInitAge];
//       }

//       // Calculate average cigarettes smoked per day for the individual  
//       gdPersonsAvgCPD = dSumOfCpd / (double)wYearsAsSmoker;

//    } catch(SimException ex) {
//       ex.AddCallPath("CalcCigarettesPerDay()");
//       throw ex;
//    }
// }


// Switching algorithm documentation
void Smoking_Simulator::CalcCigarettesPerDaySwitch() {

   // Common case: nColumns == 6 (typical smoking groups)
   // This allows compiler to better optimize loops
   // constexpr short TYPICAL_N_COLUMNS = 6;  // Unused, but kept for documentation

   short    //wIntensityLookupAge,  // Age to look up in the smoking intensity groups
            //wIntensityIndex,      // Index to start at for look up of the smoking intensity groups
            wYearsAsSmoker,       // Number of years in which person was a smoker.
            //wStartAgeInCpdData,   // The first age that has Cigarettes per day data in the table for the person's year of birth
            //wEndLoop,             // Age at which to end the uptake formula calculation
            //wLookupStartAge,      // Age to start at when getting the cigarettes per day directly from the data array
            //wPersonsYOB,          // Copy of gwPersonsYOB, when the year of birth is less than 1900, 1900 is used in the equation
            i, j, l, m, n,
            group,
            nRows,
            //finalAge,
            nColumns;
   long     lCpdStartIndex;       // Index to start at for look up of cigarettes per day
            //lCurrCpdIndex;        // Current index in cigarettes per day array
   double   //dIntensityProb,       // Probability to find in the lookup tables
            //dCpsForStartAge,      // The cigarettes per day for first age (in birth cohort) that has Cigarettes per day data
            //dUptake,              // Uptake formula results for persons current age
            //dUptakeAtCpdStart,    // Uptake formula results at age 30
            //dScalingFactor,       // (Cigarettes per day at age 30) / (Uptake formula at age 30)
            dSumOfCpd = 0,        // Sum of the annual cigarettes per day value (used to get average)
            //prob,
            roll;
            //tempSum;
   //bool     bValueFound;

   long     nValues = glCpdYOBOffset;
   nColumns = gwNumSmokingGrps;
   
   // Safety check: prevent divide-by-zero if data wasn't loaded properly
   if (nColumns <= 0) {
      throw SimException("Error", "Invalid gwNumSmokingGrps value (must be > 0). CPD data may not have loaded correctly.");
   }
   nRows = nValues / nColumns;


   // Use thread_local vectors to avoid per-call heap allocations
   // They persist across calls and only reallocate when size increases
   static thread_local std::vector<long>   cpdGroupOverLife;
   static thread_local std::vector<double> filteredCPDGroups;
   static thread_local std::vector<double> Tij;
   static thread_local std::vector<double> r0;
   static thread_local std::vector<double> r1;
   cpdGroupOverLife.resize(nRows);
   filteredCPDGroups.resize(nValues);
   Tij.resize(nColumns * nColumns);
   r0.resize(nColumns);
   r1.resize(nColumns);


   try {

      /*
      if (gdCigarettesPerDay == 0 || gdIntensityProbs == 0 || gpIndividualRNG == 0)
         throw SimException("Error", "One or more of the data components for cigarettes \nper day calculation has not been initialized.\n");
      */

      if (gwPersonsInitAge == -999)
         throw SimException("Error", "CalcCigarettesPerDay should not be called for \nindividuals that do not initiate smoking.\n");

      // Using the offset formula...
      lCpdStartIndex = (glCpdRaceOffset * (gwPersonsRace)) +
                       (glCpdSexOffset * (gwPersonsSex)) +
                       (glCpdYOBOffset * GetYOBCohortGroup(gwPersonsYOB));

      // "Filter" the gdCigarettesPerDay array based on race, gender, and cohort
      // Portable unroll hints for better performance (nColumns typically 6, nRows ~100)
      // Multiple pragmas ensure compatibility: GCC/Clang, MSVC, and standards-compliant
      #if defined(__clang__)
         #pragma clang loop unroll_count(4)
      #elif defined(__GNUC__)
         #pragma GCC unroll 4
      #elif defined(_MSC_VER)
         #pragma loop(hint_parallel(4))
      #endif
      for (i = 0; i < nRows; i++) {
         const long base_offset = lCpdStartIndex + i * nColumns;
         #if defined(__clang__)
            #pragma clang loop unroll_count(6)
         #elif defined(__GNUC__)
            #pragma GCC unroll 6
         #elif defined(_MSC_VER)
            #pragma loop(hint_parallel(6))
         #endif
         for (j = 0; j < nColumns; j++) {
            filteredCPDGroups[i * nColumns + j] = gdCigarettesPerDay[base_offset + j];
         }
      }

      // Determine number of years as a smoker
      if (gwPersonsCessAge == -999)
         wYearsAsSmoker = wSIM_CUTOFF_YEAR - (gwPersonsYOB + gwPersonsInitAge) + 1;
      else
         wYearsAsSmoker = gwPersonsCessAge - gwPersonsInitAge + 1;

      // Set up the array for storing the number of cigarettes smoked per day for ages 0 - 99 regardless?
      // Use memset for faster initialization (cpdGroupOverLife is a vector)
      std::memset(cpdGroupOverLife.data(), -999, nRows * sizeof(short));

      double term1, term2;
      static thread_local std::vector<double> switchProbs;
      switchProbs.resize(nColumns);
      double sum;

      for (i = 0; i < nRows ; i++) {

         if (i < gwPersonsInitAge) {               // if not a smoker yet
            group = -999;

         } else if (i == gwPersonsInitAge) {       // if just inititiating
            // Cumulative sum for first smoking year (typically nColumns=6)
            sum = 0;
            const long row_offset = i * nColumns;
            #pragma GCC unroll 6
            for (j = 0; j < nColumns; j++) {
               sum += filteredCPDGroups[row_offset + j];
               switchProbs[j] = sum;
            }
            
            // Binary search-style branchless group selection (faster for small nColumns)
            roll = GetNextRandForIndiv();
            group = 0;
            #pragma GCC unroll 6
            for (j = 0; j < nColumns; j++) {
               group += (roll > switchProbs[j]) ? 1 : 0;
            }
            // Clamp to valid range (branchless)
            group = (group >= nColumns) ? nColumns - 1 : group;

         } else if (i > gwPersonsInitAge) {        // if already a smoker

            // (Re)initialize Tij = 0 - use memset for better performance
            std::memset(Tij.data(), 0, nColumns * nColumns * sizeof(double));

            // Find source and target proportionality vectors for the age of interest
            const long prev_row_offset = (i - 1) * nColumns;
            const long curr_row_offset = i * nColumns;
            
            #if defined(__clang__)
               #pragma clang loop unroll_count(6)
            #elif defined(__GNUC__)
               #pragma GCC unroll 6
            #endif
            for (j = 0; j < nColumns; j++) {
               r0[j] = filteredCPDGroups[prev_row_offset + j];
               r1[j] = filteredCPDGroups[curr_row_offset + j];
            }

            // Fill out Tij - triple nested loop, optimize aggressively
            // nColumns is typically 6, so unroll hints help
            for (m = 0; m < nColumns; m++) {
               const long m_offset = m * nColumns;
               for (n = 0; n < nColumns; n++) {
                  term1 = r0[m];
                  term2 = r1[n];
                  
                  // These inner loops are small (max 6 iterations)
                  for (l = 0; l < m; l++) {
                     term2 -= Tij[l * nColumns + n];
                  }
                  for (l = 0; l < n; l++) {
                     term1 -= Tij[m_offset + l];
                  }
                  
                  // Branchless min using ternary (compiler can optimize better)
                  double min_val = (term1 < term2) ? term1 : term2;
                  Tij[m_offset + n] = (min_val < 0) ? 0 : min_val;
               }
            }

         // Normalize Tij to obtain distribution from which one can sample
         // Cache m_offset for better memory access pattern
         const long group_offset = group * nColumns;
         const double r0_group = r0[group];
         
         // Optimization: Replace division with multiplication (10-20x faster)
         const double r0_group_inv = (r0_group == 0.0) ? 0.0 : (1.0 / r0_group);
         #pragma GCC unroll 6
         for (j = 0; j < nColumns; j++) {
            switchProbs[j] = Tij[group_offset + j] * r0_group_inv;
         }

            // double sumProbs;
            // sumProbs = 0;
            // for (m = 0; m < nColumns; m++) {
            //    sumProbs += switchProbs[m];
            // }

            // Do the cumulative sum across them - optimize with early calculation
            sum = 0;
            #pragma GCC unroll 6
            for (j = 0; j < nColumns; j++) {
               sum += switchProbs[j];
               switchProbs[j] = sum;
            }

            // Pull from the distribution - binary search could be faster for large nColumns
            // but linear search is fine for nColumns=6
            roll = GetNextRandForIndiv();
            group = 0;
            #pragma GCC unroll 6
            for (j = 0; j < nColumns; j++) {
               group += (roll > switchProbs[j]) ? 1 : 0;
            }
            // Ensure group stays within bounds (can't exceed nColumns-1)
            group = (group >= nColumns) ? nColumns - 1 : group;
         } 

         cpdGroupOverLife[i] = group;
      }

      // Convert to cigarettes per day rather than category
      // Record the new CPD by age vector using pre-allocated storage
      gdPersonsCPDbyAge = gdPersonsCPDbyAgeStorage;
      for (i = 0; i < wYearsAsSmoker && i < 100; i++) {
         gdPersonsCPDbyAge[i] = -10;
      }
      short m, endAge;

      // Lookup table for CPD conversion (faster than if-else chain)
      static const double cpd_lookup[6] = {3.0, 10.0, 20.0, 30.0, 40.0, 60.0};
      
      dSumOfCpd = 0;
      if (gwPersonsCessAge == -999)
         endAge = 99;
      else
         endAge = gwPersonsCessAge;

      for (i = gwPersonsInitAge; i <= endAge; i++) {
         m = i - gwPersonsInitAge;
         short group = cpdGroupOverLife[i];
         // Bounds check and lookup (much faster than cascading if-else)
         gdPersonsCPDbyAge[m] = (group >= 0 && group < 6) ? cpd_lookup[group] : 3.0;
         dSumOfCpd += gdPersonsCPDbyAge[m];
      }
      
      // Calculate average cigarettes smoked per day for the individual  
      gdPersonsAvgCPD = dSumOfCpd / (double)wYearsAsSmoker;

   } catch(SimException ex) {
      ex.AddCallPath("CalcCigarettesPerDay()");
      throw ex;
   }
}

//Free the dynamically allocated memory
void Smoking_Simulator::Free()
{
   // Only delete data arrays if we own them (not using shared data)
   if (gbOwnsData) {
      delete [] gdInitiationProbs;    gdInitiationProbs    = 0;
      delete [] gdCessationProbs;     gdCessationProbs     = 0;
      delete [] gdMortalityProbs;     gdMortalityProbs     = 0;
      delete [] gdIntensityProbs;     gdIntensityProbs     = 0;
      delete [] gdCigarettesPerDay;   gdCigarettesPerDay   = 0;
      delete [] gwYOBCohortStartYrs;  gwYOBCohortStartYrs  = 0;
      delete [] gwYOBCohortEndYrs;    gwYOBCohortEndYrs    = 0;
   } else if (gpSharedData != 0) {
      // Release reference to shared data
      gpSharedData->release();
      gpSharedData = 0;
   }
   
   // Always delete per-instance data (but NOT gdPersonsCPDbyAge which uses static storage)
   gdPersonsCPDbyAge = 0;  // Just null the pointer, don't delete (points to gdPersonsCPDbyAgeStorage)
   delete gpRngStrategy;           gpRngStrategy        = 0;
}

// Get the age at death from a cause of death other than lung cancer.
// Probability is based on the individuals smoking status and their smoking intensity (for current and former smokers)
short Smoking_Simulator::GetAgeOfDeathFromMortality(short wStartAge, short wEndAge,
                                                   SmokingStatus eStatus, bool &bWentPastData) {

   short  wCurrentAge,
          wReturnAge = -999;
   bool   bPersonAlive = true;
   long   lMortalityOffset,
          lMortalityLocation;
   double dCurrMortalityRand,
          dCurrMortTabProb,
          dExcessRisk;
   char   sErrorMessage[300];

   try {
      bWentPastData = false;
      lMortalityOffset  = (long(gwPersonsRace) * glMortTabRaceOffset) +
                          (long(gwPersonsSex) * glMortTabSexOffset) +
                          (long(gwPersonsYOB - GetMinYearOfBirth()) * glMortTabYOBOffset);

      for (wCurrentAge = wStartAge; wCurrentAge < wEndAge && bPersonAlive && !bWentPastData; wCurrentAge++) {

         lMortalityLocation = (long(wCurrentAge-gwMinMortalityAge)*glMortTabAgeOffset) + lMortalityOffset;
         dCurrMortalityRand = GetNextMortalityRand(); //Get random value from 0 to 1 range.
         
         // Prefetch next 2 iterations
         if (__builtin_expect(wCurrentAge + 2 < wEndAge, 1)) {
            long next_location_2 = (long(wCurrentAge + 2 - gwMinMortalityAge)*glMortTabAgeOffset) + lMortalityOffset;
            __builtin_prefetch(&gdMortalityProbs[next_location_2], 0, 3);
         }

         switch (eStatus) {

            case SMKST_Never:
               // Person has not initiated, get prob of dying from other COD for person who has never smoked
               dCurrMortTabProb = gdMortalityProbs[lMortalityLocation + COL_Never]; break;

            case SMKST_Current:
               // Person is a current smoker, get their other COD prob based on their smoking status
               dCurrMortTabProb = gdMortalityProbs[lMortalityLocation + ((int)gwPersonsSmkIntensity + 1)]; break;

            case SMKST_Former:
               // Use Excess Risk for Former Smokers formula (Davis Burns et al.)
               // New in Version 3.0, program now uses the average cigarettes smoked per day for a person.
               // Optimized: cache invariant calculations to reduce redundant exp/pow calls
               {
                  // Pre-calculate time since cessation (constant per age iteration)
                  short years_since_cess = wCurrentAge - gwPersonsCessAge;
                  
                  // Calculate excess risk with optimized math
                  // Note: B3=1.08 is close to 1.0, so pow(x, 1.08) ≈ x * x^0.08
                  // But we keep exact calculation for reproducibility
                  double exponent = (B0 + B1 * gdPersonsAvgCPD + B2 * gwPersonsCessAge) * pow((double)years_since_cess, B3);
                  dExcessRisk = exp(exponent);
                  
                  // Multiply Excessive risk by difference between Current (for their smoking intenity) and Never probability
                  // then add that result to the Never Probability to get the Probability the Person will die that year
                  double prob_never = gdMortalityProbs[lMortalityLocation + COL_Never];
                  double prob_current = gdMortalityProbs[lMortalityLocation + ((int)gwPersonsSmkIntensity + 1)];
                  dCurrMortTabProb = prob_never + ((prob_current - prob_never) * dExcessRisk);
               }
               break;

            default:
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Smoking Status: %d.\n", eStatus);
               throw SimException("Error", sErrorMessage);
         }

         if (__builtin_expect(dCurrMortalityRand <= dCurrMortTabProb, 0)) {
            bPersonAlive = false;
            wReturnAge = wCurrentAge;
         }

         // If the probability was missing, it was coded as -1, mortality table 
         // checking can stop once a -1 is reached
         if (__builtin_expect(dCurrMortTabProb < 0, 0)) {
             bWentPastData = true;
         }
      }

   } catch (SimException ex) {
      ex.AddCallPath("GetAgeOfDeathFromMortality(short, short, enum, bool)");
      throw ex;
   }
   return wReturnAge;
}

// Get the minimum year of birth value
short Smoking_Simulator::GetMinYearOfBirth() {
   if (gwYOBCohortStartYrs== NULL)
      throw SimException("GetMinYearOfBirth()", 
         "Call to start year of birth cohort values (gwYOBCohortStartYrs) prior to initialization.");
   return gwYOBCohortStartYrs[0];
}

// Get the maximum year of birth value
short Smoking_Simulator::GetMaxYearOfBirth() {
   if (gwYOBCohortEndYrs == NULL)
      throw SimException("GetMaxYearOfBirth()", 
         "Call to end year of birth cohort values (gwYOBCohortEndYrs) prior to initialization.");
   return gwYOBCohortEndYrs[gwNumBirthCohorts-1];
}

// Note: GetNextCessRand, GetNextInitRand, GetNextMortalityRand, GetNextRandForIndiv
// are now inlined in smoking_sim.h for better performance

// Get the birth cohort group that the year of birth corresponds to.
// NOTE: This is now inlined in smoking_sim.h for performance. This version
// is kept for cases requiring full validation (non-hot paths).
/*
short Smoking_Simulator::GetYOBCohortGroup(short wYearBirth) {

   short wReturnValue         = -1,
         wSearchLow           = 0,
         wSearchMid,
         wSearchHigh;
   char  sErrorMessage[500];
   bool  bValueFound          = false;

   if (wYearBirth < gwYOBCohortStartYrs[0]) {
      snprintf(sErrorMessage, sizeof(sErrorMessage), "Year of Birth - %d is less than the minimum year of birth allowed - %d", \
         wYearBirth, gwYOBCohortStartYrs[0] );
      throw SimException("GetYOBCohortGroup(short)", sErrorMessage);
   }

   if ( wYearBirth > gwYOBCohortEndYrs[gwNumBirthCohorts - 1] ) {
      snprintf(sErrorMessage, sizeof(sErrorMessage), "Year of Birth - %d is greater than the maximum year of birth allowed - %d", \
         wYearBirth, gwYOBCohortEndYrs[gwNumBirthCohorts - 1]);
      throw SimException("GetYOBCohortGroup(short)", sErrorMessage);
   }

   wSearchHigh = gwNumBirthCohorts - 1;

   // Binary Search routine, constructed to look for the location where wYearBirth
   // is > gwYOBCohortStartYrs[wSearchMid] and < gwYOBCohortEndYrs[wSearchMid].
   while ((wSearchLow <= wSearchHigh) && !bValueFound) {
      wSearchMid = ( wSearchLow + wSearchHigh ) / 2;
      if ( gwYOBCohortEndYrs[wSearchMid] < wYearBirth) { //Searching too low, go higher
         wSearchLow  = wSearchMid + 1;
      } else if ( gwYOBCohortStartYrs[wSearchMid] > wYearBirth) {
         //Searching too high, go lower
         wSearchHigh = wSearchMid - 1;
      } else if ((gwYOBCohortStartYrs[wSearchMid] <= wYearBirth) && (gwYOBCohortEndYrs[wSearchMid]   >= wYearBirth)) {
         wReturnValue = wSearchMid;
         bValueFound  = true;
      }
   }

   return wReturnValue;
}
*/

// Initialize the private variables, set pointers to zero
void Smoking_Simulator::Init() {
   gwNumSexValues       = 0;
   gwNumRaceValues      = 0;
   gwNumBirthCohorts    = 0;
   gpRngStrategy        = 0;
   gdInitiationProbs    = 0;
   gdCessationProbs     = 0;
   gdMortalityProbs     = 0;
   gdIntensityProbs     = 0;
   gdCigarettesPerDay   = 0;
   gwYOBCohortStartYrs  = 0;
   gwYOBCohortEndYrs    = 0;
   gdPersonsCPDbyAge    = 0;
   gpSharedData         = 0;
   gbOwnsData           = true;

   geOutputType         = OUT_DataOnly;

   gbImmediateCessation = false;
   gwImmediateCessYear  = 0;
   
   // Data loading statistics
   glCpdRowsSkipped     = 0;
   glCpdRowsLoaded      = 0;
}

void Smoking_Simulator::setRNGStrategy(RNG_Strategy* rngStrategy) {
   delete gpRngStrategy;
   gpRngStrategy = rngStrategy;
}

// Print summary of loaded data dimensions (useful for debugging and user info)
void Smoking_Simulator::PrintDataShapeSummary() {
   SHG_STDERR( "\n=== SHG Data Shape Summary ===\n");
   SHG_STDERR( "  Races: %d, Sexes: %d\n", gwNumRaceValues, gwNumSexValues);
   SHG_STDERR( "  Birth Cohorts: %d\n", gwNumBirthCohorts);
   if (gwNumBirthCohorts > 0) {
      SHG_STDERR( "    First cohort: %d-%d\n", gwYOBCohortStartYrs[0], gwYOBCohortEndYrs[0]);
      SHG_STDERR( "    Last cohort:  %d-%d\n", 
              gwYOBCohortStartYrs[gwNumBirthCohorts-1], gwYOBCohortEndYrs[gwNumBirthCohorts-1]);
   }
   SHG_STDERR( "  Initiation ages: %d-%d\n", gwMinInitiationAge, gwMaxInitiationAge);
   SHG_STDERR( "  Cessation ages:  %d-%d\n", gwMinCessationAge, gwMaxCessationAge);
   SHG_STDERR( "  CPD ages: %ld-%ld, Intensity groups: %d\n", gwCpdMinAge, gwCpdMaxAge, gwNumIntensityGrps);
   if (glCpdRowsSkipped > 0) {
      SHG_STDERR( "  [INFO] CPD rows loaded: %ld, skipped: %ld (cohort labels not matching initiation)\n", glCpdRowsLoaded, glCpdRowsSkipped);
   } else {
      SHG_STDERR( "  CPD rows loaded: %ld\n", glCpdRowsLoaded);
   }
   SHG_STDERR( "==============================\n\n");
}

// dataMutex prevents multiple threads from accessing the data at the same time during the loading of input files
std::mutex Smoking_Simulator::dataMutex;

namespace {

// True if `path` ends with ".csv" (case-insensitive).
bool path_is_csv(const char* path) {
   if (!path) return false;
   const char* dot = strrchr(path, '.');
   if (!dot || dot[1] == '\0' || dot[2] == '\0' || dot[3] == '\0' || dot[4] != '\0') return false;
   const char c1 = dot[1], c2 = dot[2], c3 = dot[3];
   return (c1 == 'c' || c1 == 'C') && (c2 == 's' || c2 == 'S') && (c3 == 'v' || c3 == 'V');
}

} // namespace

// CSV dispatch note: LoadProbabilityData, LoadCPDFile and LoadMortalityFile below
// branch on path_is_csv() only for the header/dimension discovery step and share
// the existing data-row parsing loop.



// Read in the cigarettes per day data file, this function assumes the data
// is sorted by race, sex , YOB cohort, age and intensity group
// The data will be stored in an array that is offset by race, sex, year of birth
// age and smoking intensity level


void Smoking_Simulator::LoadCPDFile(const char* sCpdFile) {
std::lock_guard<std::mutex> lock(dataMutex);
   char     sInputLine[3001],
            sErrorMessage[500],
           *pTokenPtr            = 0;
   long     lMaxLinesExpected,
            lNumLinesRead,
            lCurrArrayLocation,
            lCpdArraySize,
            j;
   double   dCigarettesPerDay;
   long     wFirstDataLine,
            wRaceValue,
            wSexValue,
            wNumCohorts,
            wMinAgeValue,
            wMaxAgeValue,
            wCohortEndValue,
            wCohortStartValue,
            wCurrCohort,
            wNumSmokingGrps,       //Number of Smoking Intensity Groups
            wAgeValue,
            i;
   FILE    *pCpdFile     = 0;

   try {

      if (gdInitiationProbs == NULL)
         throw SimException("Error", "The initiation probability file must be loaded before the Cigarettes per day data file.\n");

      // TODO: can we easily switch between compressed and uncompressed files? Can R packages just uncompress upon loading the package?
      pCpdFile = fopen(sCpdFile, "r");
      if (pCpdFile == NULL) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "The specified input file '%s' does not exist\n or could not be opened.\n", sCpdFile);
	      throw SimException("Error", sErrorMessage);
	   }

      const bool bIsCsv = path_is_csv(sCpdFile);

      if (bIsCsv) {
         // CSV layout: single header row "RACE,SEX,START_YOB,END_YOB,AGE,CAT1,...,CATn".
         // Number of CAT* columns => intensity groups; min/max age are inferred from body rows.
         if (fgets(sInputLine, 3000, pCpdFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading CSV header of file %s", sCpdFile);
            throw SimException("Error", sErrorMessage);
         }
         sInputLine[strcspn(sInputLine, "\r\n")] = '\0';
         long nCols = 0;
         for (pTokenPtr = strtok(sInputLine, ","); pTokenPtr != NULL; pTokenPtr = strtok(NULL, ","))
            nCols++;
         if (nCols < 6) {
            snprintf(sErrorMessage, sizeof(sErrorMessage),
               "CPD CSV %s: header must be RACE,SEX,START_YOB,END_YOB,AGE followed by >=1 CAT* columns.", sCpdFile);
            throw SimException("Error", sErrorMessage);
         }
         wNumSmokingGrps = nCols - 5;

         // Pre-scan body for min/max age; dims (race, sex, cohorts) come from previously-loaded initiation.
         const long dataStart = ftell(pCpdFile);
         wMinAgeValue = LONG_MAX; wMaxAgeValue = LONG_MIN;
         while (fgets(sInputLine, 3000, pCpdFile) != NULL) {
            sInputLine[strcspn(sInputLine, "\r\n")] = '\0';
            pTokenPtr = strtok(sInputLine, ","); if (!pTokenPtr) continue;  // race
            pTokenPtr = strtok(NULL, ",");                                  // sex
            pTokenPtr = strtok(NULL, ",");                                  // start_yob
            pTokenPtr = strtok(NULL, ",");                                  // end_yob
            pTokenPtr = strtok(NULL, ",");                                  // age
            if (!pTokenPtr) continue;
            const long a = atol(pTokenPtr);
            if (a < wMinAgeValue) wMinAgeValue = a;
            if (a > wMaxAgeValue) wMaxAgeValue = a;
         }
         if (wMinAgeValue == LONG_MAX) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "No data rows in CPD CSV %s", sCpdFile);
            throw SimException("Error", sErrorMessage);
         }
         fseek(pCpdFile, dataStart, SEEK_SET);
         wRaceValue  = gwNumRaceValues;   // placate validation; not used downstream
         wSexValue   = gwNumSexValues;
         wNumCohorts = gwNumBirthCohorts;
         (void)wNumCohorts;

      } else {
         // Legacy .txt layout: line 1 = first data line, docs, dim row (race,sex,cohorts,minAge,maxAge,nIntensity).
         if (fgets(sInputLine, 3000, pCpdFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sCpdFile);
            throw SimException("Error", sErrorMessage);
         }
         pTokenPtr = strtok(sInputLine, ",");
         wFirstDataLine = atoi(pTokenPtr);

         if (wFirstDataLine <= 1) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value: %ld for location of first data line read in from file %s. Expected a value >= 2 (typically 5 for initiation/cessation, 7 for CPD). File may be missing SHG-compatible headers.",
               wFirstDataLine, sCpdFile);
            throw SimException("Error", sErrorMessage);
         }

         for (i = 2; i < wFirstDataLine; i++) {
            if (fgets(sInputLine, 3000, pCpdFile) == NULL) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Error in  file %s, End of File reached before location of first data line as specified in line 1\n", sCpdFile);
               throw SimException("Error", sErrorMessage);
            }
         }

         if (fgets(sInputLine, 3000, pCpdFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sCpdFile);
            throw SimException("Error", sErrorMessage);
         }

         pTokenPtr       = strtok(sInputLine, ",");
         wRaceValue      = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wSexValue       = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wNumCohorts     = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         (void)wNumCohorts;
         wMinAgeValue    = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wMaxAgeValue    = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wNumSmokingGrps = atoi(pTokenPtr);
      }

      gwNumSmokingGrps = wNumSmokingGrps;

      /*
      if ((wRaceValue != gwNumRaceValues) || (wSexValue != gwNumSexValues) || (wNumCohorts != gwNumBirthCohorts)) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatch between values defined from Initiation Prob Data file and this file.\n\
            Race: Init = %d, CPD = %d\nSex: Init = %d, CPD = %d\nNum Cohorts: Init = %d, CPD = %d\n", gwNumRaceValues, \
            wRaceValue, gwNumSexValues, wSexValue, gwNumBirthCohorts, wNumCohorts);
	      throw SimException("Error", sErrorMessage);
      }

      if (wNumSmokingGrps != gwNumIntensityGrps) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatch between the number of smoking intensity groups defined in the Intensity \
            Prob Data file and this file.\nIntensity file has %d groups, this file indicates %d groups.\n", gwNumIntensityGrps, \
            wNumSmokingGrps);
	      throw SimException("Error", sErrorMessage);
      }
      */

      if (wMinAgeValue < 0 || wMaxAgeValue <= 0 || wMinAgeValue >=  wMaxAgeValue) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage),"Invalid value(s) for minimum and maximum initiation ages\n read in from file %s",sCpdFile);
         throw SimException("Error", sErrorMessage);
      }

      gwCpdMinAge        = wMinAgeValue;
      gwCpdMaxAge        = wMaxAgeValue;
      gwNumIntensityGrps = gwNumSmokingGrps;
      glCpdAgeOffset     = (long)gwNumIntensityGrps;
      glCpdYOBOffset     = glCpdAgeOffset * ((gwCpdMaxAge   - gwCpdMinAge) + 1);
      glCpdSexOffset     = glCpdYOBOffset * gwNumBirthCohorts;
      glCpdRaceOffset    = glCpdSexOffset * gwNumSexValues;
      lCpdArraySize      = glCpdRaceOffset * gwNumRaceValues;
      gdCigarettesPerDay = new long double[lCpdArraySize];
      lMaxLinesExpected  = lCpdArraySize/gwNumIntensityGrps;  //All of the intesity groups are on a single line per by-group

      // 
      for (j = 0; j < lCpdArraySize; j++) {
         gdCigarettesPerDay[j] = -1;
      }

      // Read in the Probability Data Lines
      // This subroutine will
      // - read in the variable values for the line
      // - verify the values are valid (including checking the cohorts)   
      // - add the CPD value to the appropriate array location
      lNumLinesRead = 0;
      glCpdRowsSkipped = 0;
      glCpdRowsLoaded = 0;

      while (fgets(sInputLine, 3000, pCpdFile) != NULL) {
         // Drop CR/LF so the last CSV field compares as "." on Windows (CRLF) line endings;
         // otherwise ".\r" is not recognized as missing and atof can yield 0.
         sInputLine[strcspn(sInputLine, "\r\n")] = '\0';

         lNumLinesRead++;

         pTokenPtr          = strtok(sInputLine, ",");
         wRaceValue         = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wSexValue          = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wCohortStartValue  = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wCohortEndValue    = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wAgeValue          = atoi(pTokenPtr);

         wCurrCohort        = GetYOBCohortGroup(wCohortStartValue);

         // Cohort mismatch: skip row (no error) so CPD files may carry extra cohort ranges
         if (wCurrCohort < 0 || wCurrCohort >= gwNumBirthCohorts ||
             wCohortStartValue != gwYOBCohortStartYrs[wCurrCohort] ||
             wCohortEndValue != gwYOBCohortEndYrs[wCurrCohort]) {
            glCpdRowsSkipped++;
            continue;  // Skip this row
         }

         // Validate values read in
         if (wAgeValue  < gwCpdMinAge  || wAgeValue > gwCpdMaxAge ||
             wRaceValue >= gwNumRaceValues || wRaceValue < 0 ||
             wSexValue  >= gwNumSexValues || wSexValue  < 0) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid By-Variable Combination, Race = %ld, Sex = %ld, Age = %ld\n Read form file %s \
               at line number %ld", wRaceValue, wSexValue, wAgeValue, sCpdFile, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }

         // Probabilities are read in by smoking intesity group
         // Value assignment within the array is based on the offset formula
         // WriteToFile(stdout, "%d\n", lNumLinesRead);
         // if (lNumLinesRead == 1101)
            // int r = 9;

         for (i = 0; i < gwNumIntensityGrps; i++) {
            pTokenPtr = strtok(NULL, ",");
            if (strcmp(pTokenPtr, ".") != 0) {
               dCigarettesPerDay  = atof(pTokenPtr);
               lCurrArrayLocation = (glCpdRaceOffset * wRaceValue) +
                                    (glCpdSexOffset * wSexValue) +
                                    (glCpdYOBOffset * wCurrCohort) +
                                    (glCpdAgeOffset * (wAgeValue - gwCpdMinAge)) +
                                    i;

               gdCigarettesPerDay[lCurrArrayLocation] = dCigarettesPerDay;
            }
         }
         glCpdRowsLoaded++;
      }

      // Require numeric CPD intensities wherever initiation probability is strictly positive
      // (missing/`.` cells are -1). Ages with zero initiation do not need CPD rows.
      {
         const long ageBeg = (gwCpdMinAge > gwMinInitiationAge) ? gwCpdMinAge : gwMinInitiationAge;
         const long ageEnd = (gwCpdMaxAge < gwMaxInitiationAge) ? gwCpdMaxAge : gwMaxInitiationAge;
         if (ageBeg <= ageEnd) {
            for (long r = 0; r < gwNumRaceValues; r++) {
               for (long s = 0; s < gwNumSexValues; s++) {
                  for (long coh = 0; coh < gwNumBirthCohorts; coh++) {
                     for (long age = ageBeg; age <= ageEnd; age++) {
                        const long initLoc = (r * gwInitProbRaceOffset) + (s * gwInitProbSexOffset) +
                                             (coh * gwInitProbYOBOffset) + (age - gwMinInitiationAge);
                        const double dInit = gdInitiationProbs[initLoc];
                        if (dInit <= 0.0)
                           continue;
                        const long cpdBase = (glCpdRaceOffset * r) + (glCpdSexOffset * s) +
                                           (glCpdYOBOffset * coh) + (glCpdAgeOffset * (age - gwCpdMinAge));
                        bool hasNumeric = false;
                        for (int ig = 0; ig < gwNumIntensityGrps; ig++) {
                           if (gdCigarettesPerDay[cpdBase + ig] >= 0.0L) {
                              hasNumeric = true;
                              break;
                           }
                        }
                        if (!hasNumeric) {
                           snprintf(sErrorMessage, sizeof(sErrorMessage),
                                    "Initiation probability is positive (%.6g) but no numeric CPD intensity data were loaded for "
                                    "race=%ld, sex=%ld, birth cohort %d-%d, age=%ld.\n"
                                    "Check your CPD_DATA and initiation (INIT_PROB) files for consistency.\n",
                                    dInit, r, s, (int)gwYOBCohortStartYrs[coh], (int)gwYOBCohortEndYrs[coh], age);
                           throw SimException("Error", sErrorMessage);
                        }
                     }
                  }
               }
            }
         }
      }

      // Incomplete CPD grid (e.g. rows that were all dots removed from file): informational once
      // initiation-vs-CPD consistency above passes (gaps only where initiation is non-positive).
      if (lNumLinesRead < lMaxLinesExpected) {
         SHG_STDERR( "[INFO] The CPD file has fewer data rows (%ld) than the full cohort-by-age grid (%ld) implied by initiation and cessation cohort definitions.\n",
                 lNumLinesRead, lMaxLinesExpected);
         SHG_STDERR( "       After checking initiation probabilities, every missing CPD cell corresponds to an age/cohort slice with no positive initiation risk; you do not need CPD rows for those combinations.\n");
      }

      if (glCpdRowsSkipped > 0) {
         SHG_STDERR( "[INFO] CPD file: %ld rows loaded, %ld rows skipped (cohort labels not matching initiation cohorts).\n",
                 glCpdRowsLoaded, glCpdRowsSkipped);
      }

      // Relaxed validation: allow fewer lines than expected (missing cohorts are treated as no data)
      // Only error if MORE lines than expected (indicates file format issue)
      if (lNumLinesRead > lMaxLinesExpected + glCpdRowsSkipped) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Too many lines read from file %s.\n%ld were read but %ld were expected based on sex, race, birth cohort and \
            age values specified in first line of file.", sCpdFile, lNumLinesRead, lMaxLinesExpected);
         throw SimException("Error", sErrorMessage);
      }
      // End Reading in the Probabilities File
      fclose(pCpdFile);
   } catch (SimException ex) {
      if (pCpdFile != NULL)
         fclose(pCpdFile);
      ex.AddCallPath("LoadCPDFile()");
      throw ex;
   } catch (...) {
      if (pCpdFile != NULL)
         fclose(pCpdFile);
      throw SimException("LoadCPDFile()", "Unkown Error Occurred.\n");
   }
}

// Load the smoking intensity group probabilities
// The data will be stored in an array that is offset by age and smoking intensity level
void Smoking_Simulator::LoadCPDIntensityProbs(const char* sDataFileName) {

   char     sInputLine[3001],
            sErrorMessage[500],
           *pTokenPtr            = 0;
   long     lNumLinesExpected,
            lNumLinesRead,
            lCurrArrayLocation;
   double   dCurrProbability;
   short    wFirstDataLine,
            wAgeValue,
            wRaceValue,
            wSexValue,
            wNumGroups,       //Number of Smoking Intensity Groups
            wNumRaces,
            wNumSexes,
            wMinAgeValue,
            wMaxAgeValue,
            i;
   FILE    *pProbabilityFile     = 0;

   try {
      pProbabilityFile = fopen(sDataFileName, "r");
      if (pProbabilityFile == NULL) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "The specified input file '%s' does not exist\n or could not be opened.\n\n", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }

	   // Read in the first line of the file.  Line contains the line number where the data in the file begins
      // This is to allow documentation to be placed in the input file
      if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sDataFileName);
         throw SimException("Error", sErrorMessage);
      }
	   pTokenPtr = strtok(sInputLine, ",");
      wFirstDataLine = atoi(pTokenPtr);
      if (wFirstDataLine <= 1) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage),  "Invalid value: %d for location of first data line read in from file %s", \
            wFirstDataLine, sDataFileName);
	      throw SimException("Error", sErrorMessage);
      }

      // Read in the Documentation lines, If the tag Version= is found, store it in the Version Num string for the file
      for (i = 2; i < wFirstDataLine; i++) {
         if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
     	      snprintf(sErrorMessage, sizeof(sErrorMessage), "Error in  file %s, End of File reached before location of first data line as \
               specified in line 1\n", sDataFileName);
   	      throw SimException("Error", sErrorMessage);
         }
      }

      // Read in the First data line which contains the # of race values, # of sex values,
      // # of birth cohort group values, the minimum inititaion age and the maximum initiation age
      // in the order they are listed here.
      if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sDataFileName);
         throw SimException("Error", sErrorMessage);
      }

      pTokenPtr      = strtok(sInputLine, ",");
      wNumRaces      = atoi(pTokenPtr);   pTokenPtr = strtok(NULL, ",");
      wNumSexes      = atoi(pTokenPtr);   pTokenPtr = strtok(NULL, ",");
      wMinAgeValue   = atoi(pTokenPtr);   pTokenPtr = strtok(NULL, ",");
      wMaxAgeValue   = atoi(pTokenPtr);   pTokenPtr = strtok(NULL, ",");
      wNumGroups     = atoi(pTokenPtr);

      if (wNumGroups <= 0 )
         throw SimException("Error", "Invalid value read in for # of smoking intensity groups.");

      if (wMinAgeValue < 0 || wMaxAgeValue <= 0 || wMinAgeValue >=  wMaxAgeValue) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value(s) for minimum and maximum initiation ages\n read in from file %s", \
            sDataFileName);
         throw SimException("Error", sErrorMessage);
      }

      if ((wNumRaces != gwNumRaceValues) || (wNumSexes != gwNumSexValues)) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatch between number of races and number of sexes in initiation file and cohorts from CPD Intensity \
            file.\nRace: Init = %d, CPD = %d\nSex: Init = %d, CPD = %d\n", gwNumRaceValues, wNumRaces, gwNumSexValues, wNumSexes);
	      throw SimException("Error", sErrorMessage);
      }

      gwNumIntensityGrps = wNumGroups;
      gwIntensityMinAge = wMinAgeValue;
      gwIntensityMaxAge = wMaxAgeValue;

      gwIntensityAgeOffset = wNumGroups;
      gwIntensitySexOffset = ((wMaxAgeValue - wMinAgeValue) + 1) * gwIntensityAgeOffset;
      gwIntensityRaceOffset = (wNumSexes * gwIntensitySexOffset);

      gdIntensityProbs = new double[long(wNumRaces) * long(gwIntensityRaceOffset)];
      lNumLinesExpected = long((gwIntensityMaxAge - gwIntensityMinAge) + 1);

      // Read in the Probability Data Lines
      lNumLinesRead = 0;
      while (fgets(sInputLine, 3000, pProbabilityFile) != NULL) {
         lNumLinesRead++;
         pTokenPtr = strtok(sInputLine, ",");
         wRaceValue = atoi(pTokenPtr);    pTokenPtr = strtok(NULL, ",");
         wSexValue = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wAgeValue = atoi(pTokenPtr);

         // Validate values read in
         if (wRaceValue > wNumRaces) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Race Value: %d\n Read from file %s at line number %ld", wRaceValue, 
               sDataFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }
         if (wSexValue > wNumSexes) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Race Value: %d\n Read from file %s at line number %ld", wSexValue, \
               sDataFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }
         if (wAgeValue < gwIntensityMinAge || wAgeValue > gwIntensityMaxAge) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Age Value: %d\n Read from file %s at line number %ld", wAgeValue, \
               sDataFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }

         // Probabilities are read in by intensity group
         // Value assignment within the array is based on the offset formula
         for (i = 0; i < gwNumIntensityGrps; i++) {
            pTokenPtr = strtok(NULL, ",");
            if (strcmp(pTokenPtr, ".") != 0) {
               dCurrProbability  = atof(pTokenPtr);
               if ((dCurrProbability < 0) || (dCurrProbability > 1)) {
                  snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Probability: %f read for Age : %d ,Intensity Group : %d\nRead \
                     from file %s at line number %ld.\n", dCurrProbability, wAgeValue, i, sDataFileName, lNumLinesRead);
                  throw SimException("Error",sErrorMessage);
               }
            } else {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Value missing for Age : %d ,Intensity Group : %d\nValue must contain a decimal palce.\n", wAgeValue,i);
               throw SimException("Error", sErrorMessage);
            }

            // Offset formula
            lCurrArrayLocation = (wRaceValue * gwIntensityRaceOffset) + \
                                 (wSexValue * gwIntensitySexOffset) + \
                                 ((wAgeValue - gwIntensityMinAge) * gwIntensityAgeOffset) + \
                                 i;

            // Values stored as a cumulative probability
            if (i == 0) {
               gdIntensityProbs[lCurrArrayLocation] = dCurrProbability;
            } else {
               gdIntensityProbs[lCurrArrayLocation] = gdIntensityProbs[lCurrArrayLocation-1] + dCurrProbability;
            }
         }
      }

      if (lNumLinesRead < lNumLinesExpected) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Not enough lines read from file %s.\n%ld were read but %ld were expected based on sex, race, birth cohort \
            and age values specified in first line of file.", sDataFileName, lNumLinesRead, lNumLinesExpected);
         throw SimException("Error", sErrorMessage);
      }

      // End Reading in the Probabilities File
      fclose(pProbabilityFile);

   } catch(SimException ex) {
      if (pProbabilityFile != NULL) {
         fclose(pProbabilityFile);
      }
      ex.AddCallPath("LoadCPDIntensityProbs()");
      throw ex;
   } catch (...) {
      if (pProbabilityFile != NULL) {
         fclose(pProbabilityFile);
      }
      throw SimException("LoadCPDIntensityProbs()", "Unkown Error Occurred.\n");
   }
}

// Load the probability initiation/cessation data files.
void Smoking_Simulator::LoadProbabilityData(const char* sDataFileName, DataType eFileType) {
   std::lock_guard<std::mutex> lock(dataMutex); // Lock the mutex to ensure thread safety

   char     sInputLine[3001],
            sErrorMessage[500],
           *pTokenPtr            = 0;
   long     lNumLinesExpected,
            lNumLinesRead,
            lCurrArrayLocation;
   double   dCurrProbability;
   short    wFirstDataLine,
            wSexValue,
            wRaceValue,
            wAgeValue,
            wCohortValue,
            wMinAgeValue,
            wMaxAgeValue,
            i;
   FILE     *pProbabilityFile     = 0;
   // CSV-only: temp cohort labels parsed from the column-header row; copied
   // into / validated against gwYOBCohort{Start,End}Yrs after allocation.
   std::vector<short> cohortStartTmp, cohortEndTmp;


   try {

      if ((eFileType != DATA_Initiation) && (eFileType != DATA_Cessation))
         throw SimException("Error", "Invalid File Type supplied to function.");

      if (eFileType == DATA_Cessation && (gdInitiationProbs==NULL)) {
         throw SimException("Error",
            "Attempt to load Cessation Probabilities before Initiation probabilities.\nInitiation data must be loaded first.\n");
      }

      pProbabilityFile = fopen(sDataFileName, "r");
      if (pProbabilityFile == NULL) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "The specified input file '%s' does not exist\n or could not be opened.\n\n", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }

      const bool bIsCsv = path_is_csv(sDataFileName);

      if (bIsCsv) {
         // CSV layout: single header row "RACE,SEX,AGE,<cohort columns>".
         // Cohort columns are single years (e.g. "1908") or ranges ("1908-1908").
         // Min/max age and the number of races / sexes are inferred from body rows.
         if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading CSV header of file %s", sDataFileName);
            throw SimException("Error", sErrorMessage);
         }
         // Skip optional UTF-8 BOM + trailing CRLF.
         char* pHdr = sInputLine;
         if ((unsigned char)pHdr[0] == 0xEF && (unsigned char)pHdr[1] == 0xBB && (unsigned char)pHdr[2] == 0xBF) pHdr += 3;
         pHdr[strcspn(pHdr, "\r\n")] = '\0';
         pTokenPtr = strtok(pHdr, ",");   // RACE
         pTokenPtr = strtok(NULL, ",");   // SEX
         pTokenPtr = strtok(NULL, ",");   // AGE
         while ((pTokenPtr = strtok(NULL, ",")) != NULL) {
            const char* pDash = strchr(pTokenPtr, '-');
            const short yStart = (short)atoi(pTokenPtr);
            const short yEnd   = pDash ? (short)atoi(pDash + 1) : yStart;
            cohortStartTmp.push_back(yStart);
            cohortEndTmp.push_back(yEnd);
         }
         wCohortValue = (short)cohortStartTmp.size();
         if (wCohortValue <= 0) {
            snprintf(sErrorMessage, sizeof(sErrorMessage),
               "CSV header in %s must have RACE,SEX,AGE followed by >=1 cohort column.", sDataFileName);
            throw SimException("Error", sErrorMessage);
         }

         // Pre-scan body rows for race/sex/age bounds, then rewind to first data row.
         const long dataStart = ftell(pProbabilityFile);
         short maxR = 0, maxS = 0, minA = SHRT_MAX, maxA = 0;
         while (fgets(sInputLine, 3000, pProbabilityFile) != NULL) {
            pTokenPtr = strtok(sInputLine, ","); if (!pTokenPtr) continue;
            const short r = (short)atoi(pTokenPtr);
            pTokenPtr = strtok(NULL, ",");       if (!pTokenPtr) continue;
            const short s = (short)atoi(pTokenPtr);
            pTokenPtr = strtok(NULL, ",");       if (!pTokenPtr) continue;
            const short a = (short)atoi(pTokenPtr);
            if (r > maxR) maxR = r;
            if (s > maxS) maxS = s;
            if (a < minA) minA = a;
            if (a > maxA) maxA = a;
         }
         if (minA == SHRT_MAX) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "No data rows in CSV file %s", sDataFileName);
            throw SimException("Error", sErrorMessage);
         }
         wRaceValue   = (short)(maxR + 1);
         wSexValue    = (short)(maxS + 1);
         wMinAgeValue = minA;
         wMaxAgeValue = maxA;
         fseek(pProbabilityFile, dataStart, SEEK_SET);

      } else {
         // Legacy .txt layout: line 1 = first data line, docs, dim row (race,sex,cohorts,minAge,maxAge).
         if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sDataFileName);
            throw SimException("Error", sErrorMessage);
         }

         pTokenPtr = strtok(sInputLine, ",");
         wFirstDataLine = atoi(pTokenPtr);
         if (wFirstDataLine <= 1) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value: %d for location of first data line read in from file %s. Expected a value >= 2 (typically 5 for initiation/cessation, 7 for CPD). File may be missing SHG-compatible headers.",
               wFirstDataLine, sDataFileName);
            throw SimException("Error", sErrorMessage);
         }

         for (i = 2; i < wFirstDataLine; i++) {
            if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Error in  file %s, End of File reached before location of first data line as specified in line 1\n", sDataFileName);
               throw SimException("Error", sErrorMessage);
            }
         }

         if (fgets(sInputLine, sizeof(sInputLine), pProbabilityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sDataFileName);
            throw SimException("Error", sErrorMessage);
         }

         pTokenPtr = strtok(sInputLine, ",");
         wRaceValue = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ",");
         wSexValue = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ",");
         wCohortValue = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ",");
         wMinAgeValue = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ",");
         wMaxAgeValue = atoi(pTokenPtr);
      }

      if ((eFileType == DATA_Initiation) && (wRaceValue <= 0 || wSexValue <= 0 || wCohortValue <= 0)) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value read in for dimensions from file %s. Race: %d, Sex: %d, Cohorts: %d. Expected all values > 0. Check that the dimensions line (line %d) is correctly formatted as: race_count,sex_count,cohort_count,min_age,max_age", \
            sDataFileName, wRaceValue, wSexValue, wCohortValue, wFirstDataLine);
         throw SimException("Error", sErrorMessage);
      }

      if ((eFileType == DATA_Cessation) &&
         ((wRaceValue != gwNumRaceValues) || (wSexValue != gwNumSexValues) || (wCohortValue != gwNumBirthCohorts))) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatch between cohort values from Initiation and Cessation Files.\n\
            Race: Init = %d, Cess = %d\nSex: Init = %d, Cess = %d\nNum Cohorts: Init = %d, Cess = %d\n", \
            gwNumRaceValues, wRaceValue, gwNumSexValues, wSexValue, gwNumBirthCohorts, wCohortValue);
	      throw SimException("Error", sErrorMessage);
      }

      if (wMinAgeValue < 0 || wMaxAgeValue <= 0 || wMinAgeValue >=  wMaxAgeValue) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value(s) for minimum and maximum initiation ages\n read in from file %s", sDataFileName);
         throw SimException("Error", sErrorMessage);
      }

      // Load private members from Initiation data
      if (eFileType == DATA_Initiation) {
         gwNumRaceValues      = wRaceValue;
         gwNumSexValues       = wSexValue;
         gwNumBirthCohorts    = wCohortValue;
         gwMinInitiationAge   = wMinAgeValue;
         gwMaxInitiationAge   = wMaxAgeValue;
         gwInitProbYOBOffset  = (gwMaxInitiationAge - gwMinInitiationAge) + 1;
         gwInitProbSexOffset  = gwNumBirthCohorts * gwInitProbYOBOffset;
         gwInitProbRaceOffset = gwNumSexValues * gwInitProbSexOffset;
         gdInitiationProbs    = new double[(long(gwNumRaceValues) * long(gwInitProbRaceOffset))];
         {
            const long initSz = long(gwNumRaceValues) * long(gwInitProbRaceOffset);
            for (long zi = 0; zi < initSz; zi++)
               gdInitiationProbs[zi] = -1.0;
         }
         gwYOBCohortStartYrs  = new short [gwNumBirthCohorts];
         gwYOBCohortEndYrs    = new short [gwNumBirthCohorts];
         lNumLinesExpected    = long(gwNumSexValues * gwNumRaceValues *
                                   ((gwMaxInitiationAge - gwMinInitiationAge) + 1));

      // Load private members from Cessation data
      } else {
         gwMinCessationAge    = wMinAgeValue;
         gwMaxCessationAge    = wMaxAgeValue;
         gwCessProbYOBOffset  = (gwMaxCessationAge - gwMinCessationAge) + 1;
         gwCessProbSexOffset  = gwNumBirthCohorts * gwCessProbYOBOffset;
         gwCessProbRaceOffset = gwNumSexValues * gwCessProbSexOffset;

         gdCessationProbs     = new double[(long(gwNumRaceValues) * long(gwCessProbRaceOffset))];
         {
            const long cessSz = long(gwNumRaceValues) * long(gwCessProbRaceOffset);
            for (long zi = 0; zi < cessSz; zi++)
               gdCessationProbs[zi] = -1.0;
         }
         lNumLinesExpected    = long(gwNumSexValues * gwNumRaceValues *
                                   ((gwMaxCessationAge - gwMinCessationAge) + 1));
      }

      // Populate gwYOBCohort{Start,End}Yrs from either the legacy cohort header row
      // or from the CSV column-header temp arrays parsed above.
      if (bIsCsv) {
         for (i = 0; i < gwNumBirthCohorts; i++) {
            const short yStart = cohortStartTmp[i];
            const short yEnd   = cohortEndTmp[i];
            if (eFileType == DATA_Initiation) {
               if (yStart < 0 || yEnd <= 0 || yStart > yEnd) {
                  snprintf(sErrorMessage, sizeof(sErrorMessage),
                     "Invalid Year of Birth Cohort value(s).\nStart Year = %d, End Year = %d.\nRead in from file %s for cohort range: %d\n\n\n",
                     yStart, yEnd, sDataFileName, i);
                  throw SimException("Error", sErrorMessage);
               }
               gwYOBCohortStartYrs[i] = yStart;
               gwYOBCohortEndYrs[i]   = yEnd;
            } else if (yStart != gwYOBCohortStartYrs[i] || yEnd != gwYOBCohortEndYrs[i]) {
               snprintf(sErrorMessage, sizeof(sErrorMessage),
                  "Mismatching cohorts between Initiation and Cessation files at column %d: init=%d-%d, cess=%d-%d.",
                  i, gwYOBCohortStartYrs[i], gwYOBCohortEndYrs[i], yStart, yEnd);
               throw SimException("Error", sErrorMessage);
            }
         }
      } else {
         // Legacy .txt: second header line "Race,Sex,Age,YYYY-YYYY,YYYY-YYYY,..."
         if (fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading second DATA line of file %s", sDataFileName);
            throw SimException("Error", sErrorMessage);
         }
         pTokenPtr = strtok(sInputLine, ",");
         pTokenPtr = strtok(NULL, ",");
         pTokenPtr = strtok(NULL, ",");
         // NOTE: Header must use range format "YYYY-YYYY" (e.g., "1908-1908"), not a single year "1908".
         for (i = 0; i < gwNumBirthCohorts; i++) {
            pTokenPtr = strtok(NULL, "-");
            if (pTokenPtr == NULL) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Error parsing cohort header at position %d. Expected format 'YYYY-YYYY' (e.g., '1908-1908') but found invalid or missing value. File: %s. This usually means the file header uses single-year format (e.g., '1908') instead of range format. Please convert headers to use range format.", i, sDataFileName);
               throw SimException("Error", sErrorMessage);
            }
            wCohortValue = atoi(pTokenPtr);
            if (wCohortValue == 0 && pTokenPtr[0] != '0') {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Error parsing cohort start year at position %d. Could not parse '%s' as integer. File: %s. Header may be missing or in wrong format.", i, pTokenPtr, sDataFileName);
               throw SimException("Error", sErrorMessage);
            }
            if (eFileType == DATA_Initiation) {
               gwYOBCohortStartYrs[i] = wCohortValue;
            } else if (wCohortValue != gwYOBCohortStartYrs[i]) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatching starting cohorts between Initiation and Cessation probability files\nFor range : 1\n%d read from initiation file.\n%d read from cessation file.",
                  gwYOBCohortStartYrs[i], wCohortValue);
               throw SimException("Error", sErrorMessage);
            }

            pTokenPtr = strtok(NULL, ",");
            if (pTokenPtr == NULL) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Error parsing cohort end year at position %d. Expected format 'YYYY-YYYY' but found invalid or missing value. File: %s", i, sDataFileName);
               throw SimException("Error", sErrorMessage);
            }
            wCohortValue = atoi(pTokenPtr);
            if (eFileType == DATA_Initiation) {
               gwYOBCohortEndYrs[i] = wCohortValue;
            } else if (wCohortValue != gwYOBCohortEndYrs[i]) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatching starting cohorts between Initiation and Cessation probability files\nFor range : 1\n%d read from initiation file.\n%d read from cessation file.", gwYOBCohortEndYrs[i], wCohortValue);
               throw SimException("Error", sErrorMessage);
            }

            if ((eFileType == DATA_Initiation) &&
                (gwYOBCohortStartYrs[i] < 0 || gwYOBCohortEndYrs[i] <= 0 || gwYOBCohortStartYrs[i] > gwYOBCohortEndYrs[i])) {
               snprintf(sErrorMessage, sizeof(sErrorMessage),
                  "Invalid Year of Birth Cohort value(s).\nStart Year = %d, End Year = %d.\nRead in from file %s for cohort range: %d\n\n\n",
                  gwYOBCohortStartYrs[i], gwYOBCohortEndYrs[i], sDataFileName, i);
               throw SimException("Error", sErrorMessage);
            }
         }
      }

      // Read in the Probability Data Lines
      lNumLinesRead = 0;
      while (fgets(sInputLine, 3000, pProbabilityFile) != NULL) {
         lNumLinesRead++;
         pTokenPtr  = strtok(sInputLine, ",");
         wRaceValue = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ",");
         wSexValue = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ",");
         wAgeValue = atoi(pTokenPtr);

         // Validate values read in
         if (wAgeValue  < wMinAgeValue || wAgeValue > wMaxAgeValue || wRaceValue >= gwNumRaceValues || wRaceValue < 0 ||
            wSexValue  >= gwNumSexValues  || wSexValue  < 0) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid By-Variable Combination, Race = %d, Sex = %d, Age = %d\n Read form file %s at line \
               number %ld", wRaceValue, wSexValue, wAgeValue, sDataFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }

         // Probabilities are read in by year of birth cohorts
         // Values will be assigned to the probability array that corresponds to eFileType,
         // Value assignment within the array is based on the offset formula
         for (i = 0; i < gwNumBirthCohorts; i++) {
            pTokenPtr = strtok(NULL, ",");

            if (strcmp(pTokenPtr, ".") != 0) {
               dCurrProbability  = atof(pTokenPtr);
               if ((dCurrProbability < 0) || (dCurrProbability > 1)) {
                  snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Probability: %f read for Birth Cohort: %d - %d\nRead from file %s at line \
                     number %ld.\n", dCurrProbability, gwYOBCohortStartYrs[i], gwYOBCohortEndYrs[i], sDataFileName, lNumLinesRead);
                  throw SimException("Error", sErrorMessage);
               }
            } else {
               dCurrProbability = -1;
            }

            if (eFileType == DATA_Initiation) {
               lCurrArrayLocation = (wRaceValue * gwInitProbRaceOffset) + (wSexValue * gwInitProbSexOffset) +
                                     (i * gwInitProbYOBOffset)                + (wAgeValue - gwMinInitiationAge);
               gdInitiationProbs[lCurrArrayLocation] = dCurrProbability;
            } else {
               lCurrArrayLocation = (wRaceValue * gwCessProbRaceOffset) + (wSexValue * gwCessProbSexOffset) +
                                     (i * gwCessProbYOBOffset)                + (wAgeValue - gwMinCessationAge);
               gdCessationProbs[lCurrArrayLocation] = dCurrProbability;
            }
         }
      }

      if (lNumLinesRead < lNumLinesExpected) {
         SHG_STDERR( "[WARNING] Not enough data lines in %s: read %ld lines but full grid implies %ld. "
                         "Missing race/sex/age combinations remain at probability -1 (same as legacy '.' cells).\n",
                 sDataFileName, lNumLinesRead, lNumLinesExpected);
      }

      // End Reading in the Probabilities File
      fclose(pProbabilityFile);

   } catch(SimException ex) {
      if (pProbabilityFile != NULL)
         fclose(pProbabilityFile);
      ex.AddCallPath("LoadProbabilityData()");
      throw ex;
   } catch (...) {
      if (pProbabilityFile != NULL)
         fclose(pProbabilityFile);
      throw SimException("LoadProbabilityData()", "Unkown Error Occurred.\n");
   }
}

// Load mortality probabilities (all-cause, other-cause excluding lung cancer, etc.).
// Renamed from LoadOtherCODFile — "OCD" suggested a single other-cause table; both ACM and OCM-style files use this path.
void Smoking_Simulator::LoadMortalityFile(const char* sMortalityFileName) {
std::lock_guard<std::mutex> lock(dataMutex);
   char     sInputLine[1001],
            sErrorMessage[500],
           *pTokenPtr            = 0;
   long     lMaxNumLines,
            lNumLinesRead,
            lCurrArrayLocation,
            lSizeOfMortalityTable,
            j;
   double   dCurrProbability;
   short    wFirstDataLine,
            wSexValue,
            wRaceValue,
            wYearValue,
            wAgeValue,
            i;
   FILE    *pMortalityFile     = 0;

   try {
      if (gdInitiationProbs == NULL)
         throw SimException("Error", "Initiation Probabilies must be loaded before the mortality probabilities.\n");

      pMortalityFile = fopen(sMortalityFileName, "r");
      if (pMortalityFile == NULL) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "The specified input file '%s' does not exist\n or could not be opened.\n\n", sMortalityFileName);
	      throw SimException("Error", sErrorMessage);
	   }

      const bool bIsCsv = path_is_csv(sMortalityFileName);

      if (bIsCsv) {
         // CSV layout: single header row "RACE,SEX,YOB,AGE,NS,CS_CAT1,...".
         // Min/max year and min/max age are inferred from body rows; number of
         // smoking-status columns is fixed (COL_NumColumns).
         if (fgets(sInputLine, 3000, pMortalityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading CSV header of file %s", sMortalityFileName);
            throw SimException("Error", sErrorMessage);
         }
         const long dataStart = ftell(pMortalityFile);
         short minY = SHRT_MAX, maxY = 0, minA = SHRT_MAX, maxA = 0;
         while (fgets(sInputLine, 3000, pMortalityFile) != NULL) {
            pTokenPtr = strtok(sInputLine, ","); if (!pTokenPtr) continue;  // race
            pTokenPtr = strtok(NULL, ",");                                  // sex
            pTokenPtr = strtok(NULL, ",");       if (!pTokenPtr) continue;  // yob
            const short y = (short)atoi(pTokenPtr);
            pTokenPtr = strtok(NULL, ",");       if (!pTokenPtr) continue;  // age
            const short a = (short)atoi(pTokenPtr);
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
            if (a < minA) minA = a;
            if (a > maxA) maxA = a;
         }
         if (minY == SHRT_MAX) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "No data rows in mortality CSV %s", sMortalityFileName);
            throw SimException("Error", sErrorMessage);
         }
         gwMinMortalityYear = minY;
         gwMinMortalityAge  = minA;
         gwMaxMortalityAge  = maxA;
         fseek(pMortalityFile, dataStart, SEEK_SET);

      } else {
         // Legacy .txt layout: line 1 = first data line, docs, dim row.
         if (fgets(sInputLine, 3000, pMortalityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sMortalityFileName);
            throw SimException("Error", sErrorMessage);
         }

         pTokenPtr      = strtok(sInputLine, ",");
         wFirstDataLine = atoi(pTokenPtr);
         if (wFirstDataLine <= 1) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value: %d for location of first data line to read in from file %s",
               wFirstDataLine, sMortalityFileName);
            throw SimException("Error", sErrorMessage);
         }

         for (i = 2; i < wFirstDataLine; i++) {
            if (fgets(sInputLine, 3000, pMortalityFile) == NULL) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Error in  file %s, End of File reached before location of first data line as specified in line 1\n",
                  sMortalityFileName);
               throw SimException("Error", sErrorMessage);
            }
         }

         if (fgets(sInputLine, 3000, pMortalityFile) == NULL) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Error reading first DATA line of file %s", sMortalityFileName);
            throw SimException("Error", sErrorMessage);
         }

         pTokenPtr          = strtok(sInputLine, ",");
         wRaceValue         = atoi(pTokenPtr);
         pTokenPtr          = strtok(NULL, ",");
         wSexValue          = atoi(pTokenPtr);
         pTokenPtr          = strtok(NULL, ",");
         gwMinMortalityYear = atoi(pTokenPtr);
         pTokenPtr          = strtok(NULL, ",");
         gwMaxMortalityYear = atoi(pTokenPtr);
         pTokenPtr          = strtok(NULL, ",");
         gwMinMortalityAge  = atoi(pTokenPtr);
         pTokenPtr          = strtok(NULL, ",");
         gwMaxMortalityAge  = atoi(pTokenPtr);
         (void)wRaceValue; (void)wSexValue;
      }

      gwMaxMortalityYear = 2300;

      /*
      if ((wRaceValue != gwNumRaceValues) || (wSexValue != gwNumSexValues) ||
         (gwMinMortalityYear > GetMinYearOfBirth()) || (gwMaxMortalityYear < GetMaxYearOfBirth())) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Mismatch between cohort values from mortality file and cohorts from Initiation file.\
            \nRace: Init = %d, Life = %d\nSex: Init = %d, Life = %d\nMin Year Birth: Init = %d, Life = %d\nMax Year Birth: \
            Init = %d, Life = %d\n", gwNumRaceValues, wRaceValue, gwNumSexValues, wSexValue, GetMinYearOfBirth(), \
            gwMinMortalityYear, GetMaxYearOfBirth(), gwMaxMortalityYear);
	      throw SimException("Error", sErrorMessage);
      }
      */

      if (gwMinMortalityAge < 0 || gwMaxMortalityAge <= 0 || gwMinMortalityAge >=  gwMaxMortalityAge) {
	      snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value(s) for minimum and maximum initiation ages\n read in from file %s", sMortalityFileName);
         throw SimException("Error", sErrorMessage);
      }

      // Load private members from mortality data
      glMortTabAgeOffset  = long(COL_NumColumns);
      glMortTabYOBOffset  = long(((gwMaxMortalityAge - gwMinMortalityAge) + 1) * glMortTabAgeOffset);
      glMortTabSexOffset  = long(((gwMaxMortalityYear - gwMinMortalityYear) + 1) * glMortTabYOBOffset);
      glMortTabRaceOffset = long(gwNumSexValues) * glMortTabSexOffset;
      lSizeOfMortalityTable    = long(gwNumRaceValues) * long(glMortTabRaceOffset);
      gdMortalityProbs    = new double[lSizeOfMortalityTable];
      lMaxNumLines        = long(gwNumRaceValues * gwNumSexValues *
                                 ((gwMaxMortalityYear - gwMinMortalityYear)+1) *
                                 ((gwMaxMortalityAge - gwMinMortalityAge) + 1));

      // Fill in all gdMortalityProbs entries with -1
      for (j=0; j<lSizeOfMortalityTable; j++) {
         gdMortalityProbs[j] = -1;
      }

      // Read in the Probability Data Lines
      lNumLinesRead = 0;
      while (fgets(sInputLine, 3000, pMortalityFile)!=NULL) {

         lNumLinesRead++;
         pTokenPtr  = strtok(sInputLine, ",");
         wRaceValue = atoi(pTokenPtr);
         pTokenPtr  = strtok(NULL, ",");
         wSexValue  = atoi(pTokenPtr);
         pTokenPtr  = strtok(NULL, ",");
         wYearValue  = atoi(pTokenPtr);
         pTokenPtr  = strtok(NULL, ",");
         wAgeValue  = atoi(pTokenPtr);

         // Validate values read in
         if (wAgeValue  < gwMinMortalityAge    || wAgeValue > gwMaxMortalityAge ||
            wRaceValue >= gwNumRaceValues      || wRaceValue < 0                ||
            wSexValue  >= gwNumSexValues       || wSexValue  < 0                ||
            wYearValue > gwMaxMortalityYear   || wYearValue < gwMinMortalityYear) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid By-Variable Combination, Race = %d, Sex = %d, Year = %d, Age = %d\n Read form file %s \
               at line number %ld", wRaceValue, wSexValue, wYearValue, wAgeValue, sMortalityFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }

         // Probabilities are read in by smoking status type
         // Value assignment within the array is based on the offset formula
         for (i = 0; i < COL_NumColumns; i++) {
            pTokenPtr = strtok(NULL, ",");
            dCurrProbability  = atof(pTokenPtr);
            if ((dCurrProbability < 0) || (dCurrProbability > 1)) {
               snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Probability: %f read for Birth Cohort: %d - %d\nRead from file %s at line number %ld.\n", \
                  dCurrProbability, gwYOBCohortStartYrs[i], gwYOBCohortEndYrs[i], sMortalityFileName, lNumLinesRead);
               throw SimException("Error", sErrorMessage);
            }
            lCurrArrayLocation = (long(wRaceValue) * glMortTabRaceOffset) +
                                 (long(wSexValue) * glMortTabSexOffset) +
                                 (long(wYearValue - gwMinMortalityYear) * glMortTabYOBOffset) +
                                 (long(wAgeValue - gwMinMortalityAge) * glMortTabAgeOffset)   +
                                  long(i);
            gdMortalityProbs[lCurrArrayLocation] = dCurrProbability;
         }
      }

      if (lNumLinesRead > lMaxNumLines) {
         snprintf(sErrorMessage, sizeof(sErrorMessage), "Too many lines read from file %s.\n%ld max were expected based on sex, race, birth cohort and age\
            values specified in first line of file.\n%ld were read in.\n", sMortalityFileName, lMaxNumLines, lNumLinesRead);
         throw SimException("Error", sErrorMessage);
      }

      // End Reading in the Probabilities File
      fclose(pMortalityFile);

   } catch (SimException ex) {
      if (pMortalityFile != NULL)
         fclose(pMortalityFile);
      ex.AddCallPath("LoadMortalityFile()");
      throw ex;
   } catch (...) {
      if (pMortalityFile != NULL)
         fclose(pMortalityFile);
      throw SimException("LoadMortalityFile()", "Unkown Error Occurred.\n");
   }
}

// This function oversamples the PRNG that creates the random numbers for the individual
// If any of the other PRNGs are to be oversampled, that should be added in here
void Smoking_Simulator::OversamplePRNGs() {
   short  i, wLoopEnd;
   if (gwPersonsInitAge == -999) //Person did not start smoking, oversample 20 numbers
      wLoopEnd = 20;
   else //Person was a smoker so a random was used to find their smoking intensity group
      wLoopEnd = 19;
   for (i=0; i < wLoopEnd; i++) {
      GetNextRandForIndiv();
   }
}


// Run the simulations from an input file
void Smoking_Simulator::RunSimulation(const char* sInputFileName, const char* sOutputFileName,
                                      bool bPrintToScreen) {

   FILE    *pInputFile  = 0,
           *pOutputFile = 0;
   short    wSex,
            wRace,
            wYOB;
   char     sCurrInputLine[101],
           *pTokenPtr = 0;

   try {

      pInputFile = fopen(sInputFileName, "r");

      if (pInputFile == NULL) {
         PrintMessage(sInputFileName);
          throw SimException("ERROR",
            "Problem opening input file. Please verify file exists and is not in use by another program.\n");
      }

      if (sOutputFileName != NULL) {
         pOutputFile = fopen(sOutputFileName,"w");
         if (pOutputFile == NULL) {
            throw SimException("ERROR",
               "Problem opening output file. Please verify file exists and is not in use by another program.\n");
         }
      }

      while (fgets(sCurrInputLine, 100, pInputFile)) {
         pTokenPtr = strtok(sCurrInputLine, ";");
         wRace = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ";");
         wSex = atoi(pTokenPtr);
         pTokenPtr = strtok(NULL, ";");
         wYOB = atoi(pTokenPtr);

         RunSimulationSingle(wRace, wSex, wYOB, pOutputFile);
         if (bPrintToScreen) {
            #ifdef IS_R
            // Probably no need to print to screen in R;
            #else
            WriteToStream(stdout);
            #endif
         }
      }

      fclose(pInputFile);
      if (pOutputFile!=0)
         fclose(pOutputFile);

   } catch (SimException ex) {
      ex.AddCallPath("RunSimulation(char*, char*, bool)");
      if (pInputFile != NULL)
         fclose(pInputFile);
      if (pOutputFile!=0)
         fclose(pOutputFile);
      throw ex;
   }

}

// Run the simulation for the race, sex and year of birth values provided.
// Results are stored in the private members gwPersonsInitAge gwPersonsCessAge
// If File* is supplied, results will be written to the stream specified.
void Smoking_Simulator::RunSimulationSingle(short wRace, short wSex, short wYearBirth, FILE* pOutStream) {

   short    wYOBCohortGroup,
            wCurrentAge          = gwMinInitiationAge,
            wAgeAtDeath;
   long     wSearchOffset;
   bool     bCanInitiate         = true,
            bForceCessation      = false,
            bPersonInitiated     = false,
            bPersonQuit          = false,
            bPassedCohortMaxAge  = false,
            bPassedMortTabMaxAge = false;
   double   dCurrInitiationRand,
            dCurrInitiationProb,
            dCurrCessationRand,
            dCurrCessationProb;
   char     sErrorMessage[500];

   try {

      // Validate Input (can be skipped for performance when inputs are pre-validated)
      if (!gbSkipValidation) {
         if ((wYearBirth < GetMinYearOfBirth()) || (wYearBirth > 2100)) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Year of Birth: %d, supplied to Smoking History Simulator.", wYearBirth);
            throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
         }

         if ( (wSex < 0) || (wSex >= gwNumSexValues) ) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Sex Value: %d, supplied to Smoking History Simulator.", wSex);
            throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
         }

         if ( (wRace < 0) || (wRace >= gwNumRaceValues) ) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Race Value: %d, supplied to Smoking History Simulator.", wRace);
            throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
         }

         if ( (wRace == 1) && (wSex == 1) ) {
            snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Race/Sex Combination: %d/%d, supplied to Smoking History Simulator.", wRace, wSex);
            throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
         }
      }

      gwPersonsRace         = wRace;
      gwPersonsSex          = wSex;
      gwPersonsYOB          = wYearBirth;
      gwPersonsInitAge      = -999;
		gwPersonsCessAge      = -999;
      gwPersonsAgeAtDeath   = -999;
      gwPersonsSmkIntensity = SMKR_Uninitialized;
      gdPersonsAvgCPD       = 0;

      wYOBCohortGroup   = GetYOBCohortGroup(gwPersonsYOB);
      
      // Pre-calculate offset components (reduces repeated multiplications in loops)
      const long race_init_offset = gwPersonsRace * gwInitProbRaceOffset;
      const long sex_init_offset = gwPersonsSex * gwInitProbSexOffset;
      const long cohort_init_offset = wYOBCohortGroup * gwInitProbYOBOffset;
      wSearchOffset = race_init_offset + sex_init_offset + cohort_init_offset;

      // Smoking Initiation Routine
      // 3 instances in which scanning the initiation loop stops
      // Person initiates smoking, person surpasses max initiation age for their cohort,
      // person surpasses overall max initiation age,
      while (__builtin_expect(!bPersonInitiated && !bPassedCohortMaxAge && (wCurrentAge <= gwMaxInitiationAge), 1)) {

         // Get Initiation Probabilities
         dCurrInitiationRand = GetNextInitRand(); // Get random value from 0 to 1 range.
         long curr_offset = (wCurrentAge - gwMinInitiationAge) + wSearchOffset;
         dCurrInitiationProb = gdInitiationProbs[curr_offset];
         
         // Prefetch next 2 iterations (sweet spot: not too aggressive, good cache hit rate)
         if (__builtin_expect(wCurrentAge + 2 <= gwMaxInitiationAge, 1)) {
            __builtin_prefetch(&gdInitiationProbs[curr_offset + 2], 0, 3);
         }

         // If ImmediateCessation is turned on, check if the current year (birth year + current age)
         // is equal to or greater than the last year before cessation begins.
         if (__builtin_expect(gbImmediateCessation && ((gwPersonsYOB + wCurrentAge) >= (gwImmediateCessYear-1)), 0)) {
            bCanInitiate = false;
         }

         if (__builtin_expect(dCurrInitiationRand <= dCurrInitiationProb && bCanInitiate, 0)) {
            gwPersonsInitAge = wCurrentAge;
            bPersonInitiated = true;
         }

         // If the probability was missing, it was coded as -1, sim can
         // stop once one of these values are reached.
         if (__builtin_expect(dCurrInitiationProb < 0 || (((wCurrentAge+1) + gwPersonsYOB) > wSIM_CUTOFF_YEAR), 0)) {
            bPassedCohortMaxAge = true;
         }

         // Increment the age if they did not initiate
         if (__builtin_expect(!bPersonInitiated, 1)) {
            wCurrentAge++;
         }
      }

      // Smoking Cessation Routine
      // Only Occurs after a Person Initiates Smoking
      bPassedCohortMaxAge = false;

      if (bPersonInitiated) {

         // Increment the persons current age(also initiation age) if less than the minimum cessation age.
         while ( wCurrentAge < gwMinCessationAge )
            wCurrentAge++;

         // Pre-calculate offset components for cessation (reduces repeated multiplications)
         const long race_cess_offset = gwPersonsRace * gwCessProbRaceOffset;
         const long sex_cess_offset = gwPersonsSex * gwCessProbSexOffset;
         const long cohort_cess_offset = wYOBCohortGroup * gwCessProbYOBOffset;
         wSearchOffset = race_cess_offset + sex_cess_offset + cohort_cess_offset;

         while (__builtin_expect(!bPersonQuit && !bPassedCohortMaxAge && (wCurrentAge <= gwMaxCessationAge), 1)) {

            // If ImmediateCessation is turned on, check if the current year (birth year + current age) is
            // equal to or greater than the last year before cessation begins.
            if (__builtin_expect(gbImmediateCessation && ((gwPersonsYOB + wCurrentAge) >= (gwImmediateCessYear-1)), 0)) {
               bForceCessation = true;
            }

            dCurrCessationRand = GetNextCessRand();
            long curr_offset = (wCurrentAge-gwMinCessationAge)+wSearchOffset;
            dCurrCessationProb = gdCessationProbs[curr_offset];
            
            // Prefetch next 2 iterations
            if (__builtin_expect(wCurrentAge + 2 <= gwMaxCessationAge, 1)) {
               __builtin_prefetch(&gdCessationProbs[curr_offset + 2], 0, 3);
            }

            if (__builtin_expect(dCurrCessationRand <= dCurrCessationProb || bForceCessation, 0)) {
               gwPersonsCessAge  = wCurrentAge;
               bPersonQuit = true;
            }

            // If the probability was missing, it was coded as -1,
            // simulation can stop once one of these values are reached.
            if (dCurrCessationProb < 0 || (((wCurrentAge+1) + gwPersonsYOB) > wSIM_CUTOFF_YEAR))
               bPassedCohortMaxAge = true;
            //Age can be incremented either way here, unlike initiation
            wCurrentAge++;
         }
      }

      // Calculate the number of cigarettes smoked per day by people who initiate smoking
      if (bPersonInitiated) {
         CalcCigarettesPerDaySwitch();
      }

      // Calculate if person dies from a Cause of Death other than lung cancer
      // Loop through their entire life and check the probablility that the
      // person will die that year based on their smoking status in that year
      // Routine to use varies based on persons smoking history

      // People who never smoke
      if (__builtin_expect(!bPersonInitiated, 0)) {
         gwPersonsAgeAtDeath = GetAgeOfDeathFromMortality(gwMinMortalityAge, gwMaxMortalityAge + 1, SMKST_Never, bPassedMortTabMaxAge);

      // People who start smoking, and never quit
      } else if (__builtin_expect(bPersonInitiated && !bPersonQuit, 1)) {
         wAgeAtDeath = GetAgeOfDeathFromMortality(gwMinMortalityAge, gwPersonsInitAge, SMKST_Never, bPassedMortTabMaxAge);
         if (__builtin_expect((wAgeAtDeath == -999) && !bPassedMortTabMaxAge, 1)) {
            wAgeAtDeath = GetAgeOfDeathFromMortality(gwPersonsInitAge,gwMaxMortalityAge+1, SMKST_Current, bPassedMortTabMaxAge);
         }
         gwPersonsAgeAtDeath = wAgeAtDeath;

      // People who start smoking and quit smoking
      } else if (bPersonInitiated && bPersonQuit) {
         wAgeAtDeath = GetAgeOfDeathFromMortality(gwMinMortalityAge,gwPersonsInitAge, SMKST_Never, bPassedMortTabMaxAge);
         if (__builtin_expect((wAgeAtDeath == -999) && !bPassedMortTabMaxAge, 1)) {
            wAgeAtDeath = GetAgeOfDeathFromMortality(gwPersonsInitAge,gwPersonsCessAge, SMKST_Current, bPassedMortTabMaxAge);
            if (__builtin_expect((wAgeAtDeath == -999) && !bPassedMortTabMaxAge, 1)) {
               wAgeAtDeath = GetAgeOfDeathFromMortality(gwPersonsCessAge,gwMaxMortalityAge+1, SMKST_Former, bPassedMortTabMaxAge);
            }
         }
         gwPersonsAgeAtDeath = wAgeAtDeath;
      }

      if (__builtin_expect(pOutStream != 0, 1))
         WriteToStream(pOutStream);

      // Oversample the PRNGs (only does the PRNG that generates Randoms for the individual)
      // Can be skipped for performance when reproducibility across different code paths isn't needed
      if (__builtin_expect(!gbSkipOversampling, 1)) {
         OversamplePRNGs();
      }

   } catch (SimException ex) {
      ex.AddCallPath("RunSimulation(short,short,short)");
      throw ex;
   }
}

// Set private class member geOutputType based on value in wOutputType
void  Smoking_Simulator::SetOutputType(short wOutputType) {
   char        sErrorMessage[500];
   OutputType  eOutputType;
   eOutputType = (OutputType)wOutputType;
   if ( (eOutputType < OUT_DataOnly) || (eOutputType >= OUT_Uninitialized)) {
      snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Value supplied for Output Type : %d", wOutputType);
      throw SimException("SetOutputType(short)", sErrorMessage);
   }
   geOutputType = eOutputType;
}

// Write the output to pOutStream in the appropriate format
void Smoking_Simulator::WriteToStream(FILE *pOutStream) {
   try {
      switch (geOutputType) {
         case OUT_TextReport:
            WriteAsText(pOutStream);
            break;
         case OUT_TimeLine:
            WriteAsTimeline(pOutStream);
            break;
         case OUT_XML_Tags:
            WriteAsXML(pOutStream);
            break;
         case OUT_DataOnly:
         default:
            WriteAsData(pOutStream);
            break;
      };
   } catch (SimException ex) {
      ex.AddCallPath("WriteToStream(FILE *pOutStream)");
      throw ex;
   }
}

// Write the results to pOutStream in a text style format
void Smoking_Simulator::WriteAsText(FILE *pOutStream) {
   short wYearsAsSmoker, i;

   if (pOutStream == 0)
      throw SimException("WriteAsText(FILE *)","Supplied output File is not open for writing.");

   WriteToFile(pOutStream, "========================================================\n");
   WriteToFile(pOutStream, " Race:            %s\n", sRACE_LABELS[gwPersonsRace]);
   WriteToFile(pOutStream, " Sex:             %s\n", sSEX_LABELS[gwPersonsSex]);
   WriteToFile(pOutStream, " Year Of Birth:   %d\n", gwPersonsYOB);

   if (gwPersonsInitAge >= 0) {
      WriteToFile(pOutStream," Initiation Age:  %d\n", gwPersonsInitAge);
      if (gwPersonsCessAge >= 0)
         WriteToFile(pOutStream, " Cessation Age:   %d\n", gwPersonsCessAge);
      else WriteToFile(pOutStream, " Cessation Age:   Person Never Quit Smoking.\n"); } else {
      WriteToFile(pOutStream, " Initiation Age:  Person Never Initiated Smoking.\n");
   }

   if (gwPersonsAgeAtDeath >= 0) {
      WriteToFile(pOutStream, " Age At Death:    %d\n", gwPersonsAgeAtDeath);
   } else {
      WriteToFile(pOutStream, " Age At Death:    Person alive through %d.\n", wSIM_CUTOFF_YEAR);
   }

   if (gwPersonsInitAge >= 0) {
      WriteToFile(pOutStream, " People are not put into a smoker category for life in SHG v2.0.");
      WriteToFile(pOutStream, " Intensity Probability : %f .\n", gdTempIntensityProb);

      if (gwPersonsCessAge == -999)
         wYearsAsSmoker = (wSIM_CUTOFF_YEAR - (gwPersonsYOB+gwPersonsInitAge)) + 1;
      else
         wYearsAsSmoker = (gwPersonsCessAge - gwPersonsInitAge) + 1;

      WriteToFile(pOutStream, " Age        Cigarettes per day\n");

      for (i=0; i<wYearsAsSmoker; i++) {
         if (i + gwPersonsInitAge < 100) {
            WriteToFile(pOutStream, " %d         %f\n", (i+gwPersonsInitAge), gdPersonsCPDbyAge[i]);
         }
      }
   }
}


//------------------------------------------------------------------------------
//Write the results to pOutStream in a timeline style format
void  Smoking_Simulator::WriteAsTimeline(FILE *pOutStream) {
   short wStopAge, i;

   if (pOutStream == 0)
      throw SimException("WriteAsTimeline(FILE *)", \
         "Supplied output File is not open for writing.");

   WriteToFile(pOutStream, "Hist !%c %c %d ", sRACE_LABELS[gwPersonsRace][0], sSEX_LABELS[gwPersonsSex][0], gwPersonsYOB);

   if (gwPersonsInitAge >= 0 && gwPersonsCessAge >= 0)
      WriteToFile(pOutStream, "%d %d ", gwPersonsInitAge, gwPersonsCessAge);
   else if (gwPersonsInitAge >= 0)
      WriteToFile(pOutStream, "%d - ", gwPersonsInitAge);
   else
      WriteToFile(pOutStream, "- - ");

   if (gwPersonsAgeAtDeath >= 0)
      WriteToFile(pOutStream, "%d\n", gwPersonsAgeAtDeath);
   else
      WriteToFile(pOutStream, "-\n");

   WriteToFile(pOutStream, "Age  !");

   for (i = 0; i < 17; i++)
      WriteToFile(pOutStream, "----+");
   wStopAge = wSIM_CUTOFF_YEAR - gwPersonsYOB;

   if (gwPersonsAgeAtDeath != 0)
      WriteToFile(pOutStream, "\n%4d !", gwPersonsYOB);
   else
      WriteToFile(pOutStream, "\n%4d X", gwPersonsYOB);

   if ( gwPersonsInitAge >= 0) {
      for (i = 1; i < gwPersonsInitAge; i++) {
         if (i != gwPersonsAgeAtDeath)
            WriteToFile(pOutStream,"-");
         else
            WriteToFile(pOutStream,"X");
      }
      if (gwPersonsCessAge >= 0) {
         for (i = gwPersonsInitAge; i < gwPersonsCessAge; i++) {
            if (i != gwPersonsAgeAtDeath)
               WriteToFile(pOutStream,"s");
            else
               WriteToFile(pOutStream,"X");
         }
         for (i = gwPersonsCessAge; i <= wStopAge; i++) {
            if (i != gwPersonsAgeAtDeath)
               WriteToFile(pOutStream, "q");
            else
               WriteToFile(pOutStream, "X");
         }
      } else {
         for (i = gwPersonsInitAge; i <= wStopAge; i++) {
            if (i != gwPersonsAgeAtDeath)
               WriteToFile(pOutStream, "s");
            else
               WriteToFile(pOutStream, "X");
            }
      }
   } else {
      for (i = 1; i <= wStopAge; i++) {
         if (i != gwPersonsAgeAtDeath)
            WriteToFile(pOutStream, "-");
         else
            WriteToFile(pOutStream, "X");
         }
      }
   WriteToFile(pOutStream,"!%d\n", wSIM_CUTOFF_YEAR);
   WriteToFile(pOutStream,"!The average cigarettes smoked per day by age is not available with this type of output\n");
}


// Write the results to pOutStream in a XML style tagged format
void  Smoking_Simulator::WriteAsXML(FILE *pOutStream) {
   short wYearsAsSmoker, i;
   if (pOutStream == 0) {
      throw SimException("WriteAsTimeline(FILE *)", "Supplied output File is not open for writing.");
   }
   WriteToFile(pOutStream, "<RESULT>\n");
   WriteToFile(pOutStream, "<INITIATION_AGE>\n%d\n</INITIATION_AGE>\n", gwPersonsInitAge);
   WriteToFile(pOutStream, "<CESSATION_AGE>\n%d\n</CESSATION_AGE>\n", gwPersonsCessAge);
   WriteToFile(pOutStream, "<MORTALITY_AGE>\n%d\n</MORTALITY_AGE>\n", gwPersonsAgeAtDeath);
   if (gwPersonsInitAge >= 0) {
      WriteToFile(pOutStream, "<SMOKING_HIST>\n");
      WriteToFile(pOutStream, "<INTENSITY>\n");
      WriteToFile(pOutStream, "Not applicable in SHG v2\n");
      WriteToFile(pOutStream, "</INTENSITY>\n");

      if (gwPersonsCessAge == -999) // Person does not quit smoking
         wYearsAsSmoker = (wSIM_CUTOFF_YEAR - (gwPersonsYOB+gwPersonsInitAge))+1;
      else
         wYearsAsSmoker = (gwPersonsCessAge - gwPersonsInitAge) + 1;

      // Print out number of age_CPD Combos to expect
      WriteToFile(pOutStream, "<AGE_CPD_COUNT>\n%d\n</AGE_CPD_COUNT>\n", wYearsAsSmoker);
      for (i = 0; i < wYearsAsSmoker; i++) {
         if (i + gwPersonsInitAge < 100) {
            WriteToFile(pOutStream, "<AGE_CPD>\n");
            WriteToFile(pOutStream, "<AGE>\n%d\n</AGE>\n", (i+gwPersonsInitAge));
            WriteToFile(pOutStream, "<CPD>\n%f\n</CPD>\n", gdPersonsCPDbyAge[i]);
            WriteToFile(pOutStream, "</AGE_CPD>\n");
         }
      }
      WriteToFile(pOutStream, "</SMOKING_HIST>\n");
   }
   WriteToFile(pOutStream, "</RESULT>\n");
}

// Write the results to pOutStream in a data style format
void Smoking_Simulator::WriteAsData(FILE *pOutStream) {
   if (pOutStream == 0) {
      throw SimException("WriteAsData(FILE *)", "Supplied output File is not open for writing.");
   }

   // Use a single buffer for all output - dramatically reduces fprintf calls
   static thread_local char buffer[16384];  // 16KB buffer per thread
   char* ptr = buffer;
   
   // Write basic fields using fast_itoa
   ptr += fast_itoa(gwPersonsRace, ptr); *ptr++ = ';';
   ptr += fast_itoa(gwPersonsSex, ptr); *ptr++ = ';';
   ptr += fast_itoa(gwPersonsYOB, ptr); *ptr++ = ';';
   ptr += fast_itoa(gwPersonsInitAge, ptr); *ptr++ = ';';
   ptr += fast_itoa(gwPersonsCessAge, ptr); *ptr++ = ';';
   ptr += fast_itoa(gwPersonsAgeAtDeath, ptr); *ptr++ = ';';

   // Print CPD data if smoker
   if (gwPersonsInitAge != -999) {
      short wYearsAsSmoker;
      if (gwPersonsCessAge == -999)
         wYearsAsSmoker = wSIM_CUTOFF_YEAR - (gwPersonsYOB + gwPersonsInitAge) + 1;
      else
         wYearsAsSmoker = gwPersonsCessAge - gwPersonsInitAge + 1;
      for (short i = 0; i < wYearsAsSmoker; i++) {
         if (i + gwPersonsInitAge < 100) {
            ptr += fast_itoa(i + gwPersonsInitAge, ptr); *ptr++ = ';';
            ptr += fast_dtoa2(gdPersonsCPDbyAge[i], ptr); *ptr++ = ';';
         }
      }
   }

   *ptr++ = '\n';
   *ptr = '\0';
   
   // Single write for entire record
   fputs(buffer, pOutStream);
}

// R / CLI: variadic file write; null stream is a no-op (avoids UB if output could not be opened)
void WriteToFile(FILE* stream, const char* format, ...) {
   if (!stream) {
      return;
   }
   va_list args;
   va_start(args, format);
   #ifdef IS_R
      vfprintf(stream, format, args);
      // TODO: do we need/want text file logging in Rcpp? Maybe for comparison with CLI or debugging purposes? Or just use Rcpp::Rcout?
      // Rcpp::Rcout << vfmt::vformat(format, args);
   #else
      vfprintf(stream, format, args);
   #endif
   va_end(args);
}

// Write an error to either R's console or to stderr
void PrintError(const char* format, ...) {
   va_list args;
   va_start(args, format);
   #ifdef IS_R
      // Use REvprintf for variadic arguments (CRAN compliant)
      REvprintf(format, args);
   #else
      vfprintf(stderr, format, args);
   #endif
   va_end(args);
}

// RCPP does not allow fprintf to be used, so this function is used to replace it
void PrintMessageFormatted(const char* format, ...) {
   va_list args;
   va_start(args, format);
   #ifdef IS_R
      // Not expecting that we will need to print to console in R but including an option just in case
      Rprintf(format, args);
   #else
      vfprintf(stdout, format, args);
      //fflush(stdout); // Ensure unbuffered output
   #endif
   va_end(args);
}

void PrintMessage(const char* message) {
   #ifdef IS_R
      Rcpp::Rcout << message;
   #else
      fprintf(stdout, "%s", message);
   #endif
}

// ============================================================
// XML Header Writing Functions (shared between CLI and R)
// ============================================================

// Writes out tagged information about the program to pOutStream
void WriteRunInfoTag(FILE* pOutStream, const char* sVersion, const char* sInitiationSeed,
                     const char* sCessSeed, const char* sMortalitySeed, const char* sMiscSeed,
                     const char* sImmediateCessYear, const char* sInitFile, const char* sCessFile,
                     const char* sMortalityProbFile, const char* sQuintilesFile, const char* sCPDDataFile,
                     const char* sOutputFile, const char* sErrorFile, const char* sRNGStrategy, 
                     const char* sRngStreamSeed, const char* sInputFileName,
                     int numSegments, int numThreads, bool multiThreaded, bool autoSegments) {
   if (pOutStream == NULL)
      throw SimException("WriteRunInfoTag()::ERROR","Output stream is not initialized.\n");

   WriteToFile(pOutStream,"<RUNINFO>\n");
   WriteToFile(pOutStream,"<VERSION>%s</VERSION>\n", sVersion);
   WriteToFile(pOutStream,"<RNGSTRATEGY>%s</RNGSTRATEGY>\n", sRNGStrategy);
   WriteToFile(pOutStream,"<SEEDS>\n");
   if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
      WriteToFile(pOutStream,"<INIT_PRNG_SEED>%s</INIT_PRNG_SEED>\n", sInitiationSeed);
      WriteToFile(pOutStream,"<CESS_PRNG_SEED>%s</CESS_PRNG_SEED>\n", sCessSeed);
      WriteToFile(pOutStream,"<MORTALITY_PRNG_SEED>%s</MORTALITY_PRNG_SEED>\n", sMortalitySeed);
      WriteToFile(pOutStream,"<MISC_PRNG_SEED>%s</MISC_PRNG_SEED>\n", sMiscSeed);
   } else {
      WriteToFile(pOutStream,"<RNGSTREAM_SEED>%s</RNGSTREAM_SEED>\n", sRngStreamSeed);
   }
   WriteToFile(pOutStream,"</SEEDS>\n");
   // Parallel processing configuration (for reproducibility)
   WriteToFile(pOutStream,"<PARALLEL>\n");
   WriteToFile(pOutStream,"<NUM_SEGMENTS>%d</NUM_SEGMENTS>\n", numSegments);
   WriteToFile(pOutStream,"<NUM_THREADS>%d</NUM_THREADS>\n", numThreads > 0 ? numThreads : 1);
   WriteToFile(pOutStream,"<MULTI_THREADED>%s</MULTI_THREADED>\n", multiThreaded ? "true" : "false");
   WriteToFile(pOutStream,"<AUTO_SEGMENTS>%s</AUTO_SEGMENTS>\n", autoSegments ? "true" : "false");
   WriteToFile(pOutStream,"</PARALLEL>\n");
   WriteToFile(pOutStream,"<DATAFILES>\n");
   WriteToFile(pOutStream,"<INPUT_FILE>%s</INPUT_FILE>\n", sInputFileName);
   WriteToFile(pOutStream,"<INITIATION>%s</INITIATION>\n", sInitFile);
   WriteToFile(pOutStream,"<CESSATION>%s</CESSATION>\n", sCessFile);
   WriteToFile(pOutStream,"<MORTALITY>%s</MORTALITY>\n", sMortalityProbFile);
   WriteToFile(pOutStream,"<CIG_PER_DAY>%s</CIG_PER_DAY>\n</DATAFILES>\n", sCPDDataFile);
   WriteToFile(pOutStream,"<OUTFILES>\n<OUTPUT>%s</OUTPUT>\n", sOutputFile);
   WriteToFile(pOutStream,"<ERRORS>%s</ERRORS>\n</OUTFILES>\n", sErrorFile);
   WriteToFile(pOutStream,"<OPTIONS>\n<CESSATION_YR>%s</CESSATION_YR>\n", sImmediateCessYear);
   WriteToFile(pOutStream,"</OPTIONS>\n</RUNINFO>\n");
}

// Writes out tagged information about the current run to pOutStream
void WriteInputTag(FILE* pOutStream, const char* sRace, const char* sSex, 
                   const char* sYearOfBirth, const char* sNumReps, bool withHoldTags) {
   int iSex, iRace;

   try {
      if (pOutStream == NULL) {
         throw SimException("ERROR","Output stream is not initialized.\n");
      }

      iSex = atoi(sSex);
      iRace = atoi(sRace);

      if (!withHoldTags) {
         WriteToFile(pOutStream, "<INPUTS>\n");

         if (iRace >= 0 && iRace < Smoking_Simulator::NUM_RACES) {
            WriteToFile(pOutStream, "<RACE>%s</RACE>\n", sRACE_LABELS[iRace]);
         } else {
            WriteToFile(pOutStream, "<RACE>\n%d\n</RACE>\n", iRace);
         }

         if (iSex >= 0 && iSex < Smoking_Simulator::NUM_SEXES) {
            WriteToFile(pOutStream,"<SEX>%s</SEX>\n", sSEX_LABELS[iSex]);
         } else {
            WriteToFile(pOutStream,"<SEX>\n%d\n</SEX>\n", iSex);
         }

         WriteToFile(pOutStream,"<YOB>%s</YOB>\n",sYearOfBirth);
         if (sNumReps != NULL && (strcmp(sNumReps,"\0") != 0)) {
            WriteToFile(pOutStream,"<REPEAT>%s</REPEAT>\n", sNumReps);
         }
         WriteToFile(pOutStream,"</INPUTS>\n");
      }
   } catch (SimException ex) {
      ex.AddCallPath("WriteInputTag(FILE*,char*...)");
      throw ex;
   }
}

void WriteSimulationOpenTag(FILE* pOutStream, bool withHoldTags) {
   if (!withHoldTags) {
      WriteToFile(pOutStream, "<SIMULATION>\n");
   }
}

void WriteSimulationCloseTag(FILE* pOutStream, bool withHoldTags) {
   if (!withHoldTags) {
      WriteToFile(pOutStream, "</RUN>\n</SIMULATION>\n");
   }
}
