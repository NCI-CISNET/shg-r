// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: smoking_sim.cpp
// Author: Martin Krapcho & Ben Racine
// E-Mail: KrapchoM@imsweb.com & ben.racine@cornerstonenw.com
// NCI Contact: Rocky Feuer

#include "smoking_sim.h"
#include <string>
#include <limits>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

using namespace std;

// Constructor
Smoking_Simulator::Smoking_Simulator(const char* sInitiationProbFile, const char* sCessationProbFile,
                                     const char* sLifeTableFile,      const char* sCpdIntensityProbFile,
                                     const char* sCpdDataFile,        unsigned long ulInitPRNGSeed,
                                     unsigned long ulCessPRNGSeed,    unsigned long ulLifeTabSeed,
                                     unsigned long ulIndivRndsSeed,   short wOutputType,
                                     short wCessationYear) {
   char sErrorMessage[300];
   try {
      Init();
      LoadProbabilityData(sInitiationProbFile, Smoking_Simulator::DATA_Initiation);
      LoadProbabilityData(sCessationProbFile, Smoking_Simulator::DATA_Cessation);
      LoadCPDFile(sCpdDataFile);
      LoadOtherCODFile(sLifeTableFile);

      InitPRNGs(ulInitPRNGSeed, ulCessPRNGSeed, ulLifeTabSeed, ulIndivRndsSeed);
      SetOutputType(wOutputType);

      // Immediate Cessation Values are initialized to 0 and false respectively, 
      // Check to see if they need to be changed.
      if ((wCessationYear != 0) || (wCessationYear >= wMIN_IMMEDIATE_CESSATION_YEAR && wCessationYear <= wSIM_CUTOFF_YEAR)) {
         gwImmediateCessYear = wCessationYear;
         gbImmediateCessation = true;
      } else if ( wCessationYear != 0) {
         sprintf(sErrorMessage, "Invalid Value for Immediate Cessation Year.\n \
            Valid values are 0 and the range %d to %d.\n", wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
         throw SimException("Error", sErrorMessage);
      }
    } catch (SimException ex) {
      ex.AddCallPath("Smoking_Simulator()");
      Free();
      throw ex;
    }
}

// Destructor
Smoking_Simulator::~Smoking_Simulator() {
   Free();
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
void Smoking_Simulator::CalcCigarettesPerDay() {

   //char     sErrorMessage[500];
   short    wIntensityLookupAge,  // Age to look up in the smoking intensity groups
            wIntensityIndex,      // Index to start at for look up of the smoking intensity groups
            wYearsAsSmoker,       // Number of years in which person was a smoker.
            wStartAgeInCpdData,   // The first age that has Cigarettes per day data in the table for the person's year of birth
            wEndLoop,             // Age at which to end the uptake formula calculation
            wLookupStartAge,      // Age to start at when getting the cigarettes per day directly from the data array
            wPersonsYOB,          // Copy of gwPersonsYOB, when the year of birth is less than 1900, 1900 is used in the equation
            i;
   long     lCpdStartIndex,       // Index to start at for look up of cigarettes per day
            lCurrCpdIndex;        // Current index in cigarettes per day array
   double   dIntensityProb,       // Probability to find in the lookup tables
            // dCpsForStartAge,      // The cigarettes per day for first age (in birth cohort) that has Cigarettes per day data
            dUptake,              // Uptake formula results for persons current age
            dUptakeAtCpdStart,    // Uptake formula results at age 30
            dScalingFactor,       // (Cigarettes per day at age 30)/(Uptake formula at age 30)
            dSumOfCpd = 0;        // Sum of the annual cigarettes per day value (used to get average)
   bool     bValueFound;

   try {

      if (gdCigarettesPerDay == 0 || gdIntensityProbs == 0 || gpIndivRndsPRNG == 0) {
         throw SimException("Error", "One or more of the data components for cigarettes \nper \
            day calculation has not been initialized.\n");
      }
      if (gwPersonsInitAge == -999) {
         throw SimException("Error", "CalcCigarettesPerDay should not be called for \nindividuals \
            that do not initiate smoking.\n");
      }

      // Get the probability for the quintile lookup
      dIntensityProb = GetNextRandForIndiv();

      // Get the age for intensity probabilities lookup
      // Initiation Ages below the min age use the min intensity age probabilities
      if (gwPersonsInitAge < gwIntensityMinAge) {      
         wIntensityLookupAge = gwIntensityMinAge;
      // Initiation Ages above the max age use the max intensity age probabilities
      } else if (gwPersonsInitAge > gwIntensityMaxAge) {
         wIntensityLookupAge = gwIntensityMaxAge;
      // Otherwise look up the initiation age
      } else {
         wIntensityLookupAge = gwPersonsInitAge;
      }

      // Set the starting point for the lookup (Age - min age) * age offset
      bValueFound = false;
      wIntensityIndex = (wIntensityLookupAge - gwIntensityMinAge) * gwIntensityAgeOffset;


      // Loop through Intensity probabilities to find quintile for person
      for (i = 0; i < (gwNumIntensityGrps - 1) && !bValueFound; i++) {
         if (dIntensityProb <  gdIntensityProbs[i + wIntensityIndex]) {
            gwPersonsSmkIntensity = (SmokingIntensity) i;
            bValueFound = true;
         }
      }
      // If the value was not found, assume that the probabilties did not correctly sum to one, 
      // and assign the person to the last quintile
      if (!bValueFound) {
         gwPersonsSmkIntensity = (SmokingIntensity)(SMKR_NumGroups - 1);
      }

      gdTempIntensityProb = dIntensityProb;

      // Set up the array for storing the number of cigarettes smoked per day by age
      if (gwPersonsCessAge == -999) { 
         // Person does not quit smoking
         wYearsAsSmoker = (wSIM_CUTOFF_YEAR - (gwPersonsYOB + gwPersonsInitAge)) + 1;
      } else {
         // Person will quit at some time
         wYearsAsSmoker = (gwPersonsCessAge - gwPersonsInitAge) + 1;
      }
      gdPersonsCPDbyAge = new double[wYearsAsSmoker];
      for ( i = 0; i < wYearsAsSmoker; i++) {
         gdPersonsCPDbyAge[i] = 0;
      }

      // Find the age at which the cigarette per day numbers begin for the persons YOB
      // In most cases this is age 30, but for those born in 1975-1979 or 1980-1984, the ages are lower (26 and 21)
      bValueFound      = false;
      lCpdStartIndex   = (glCpdRaceOffset * (gwPersonsRace)) +
                         (glCpdSexOffset  * (gwPersonsSex))  +
                         (glCpdYOBOffset  * GetYOBCohortGroup(gwPersonsYOB)) +
                         (long)gwPersonsSmkIntensity;
      lCurrCpdIndex    = lCpdStartIndex;

      while (!bValueFound) {
         if (gdCigarettesPerDay[lCurrCpdIndex] >= 0) {
            bValueFound = true;
            wStartAgeInCpdData = (short)(((lCurrCpdIndex - lCpdStartIndex) / glCpdAgeOffset) + gwCpdMinAge);
            lCpdStartIndex = lCurrCpdIndex;
         } else {
            lCurrCpdIndex += glCpdAgeOffset;
         }
      }

      // Use the uptake formula to calculate the cigarettes per day before age 30
      // The age is lower (26 and 21) for the later birth cohorts (1975-1979 and 1980-1984)
      // In the notes below only age 30 will be referenced but it applies to ages 26 & 21 when necessary
      if (gwPersonsInitAge < wStartAgeInCpdData) {

         if ( gwPersonsYOB >= 1900) {
            wPersonsYOB = gwPersonsYOB;
         } else {
            wPersonsYOB = 1900;
         }

         // Get age at which to stop the uptake loop
         wEndLoop = min(wStartAgeInCpdData, (short)(gwPersonsInitAge + wYearsAsSmoker));

         // Calculate the uptake formulas value at the age where the cigarette per day numbers begin
         if (gwPersonsSex == SEX_Male) {
            dUptakeAtCpdStart = -38.578 + (3.342 * (sqrt(wStartAgeInCpdData - gwPersonsInitAge))) -
                                (0.00168 * pow(max(79, ((wPersonsYOB + wStartAgeInCpdData) - 1900 )), 2)) -
                                (17.538 * sqrt(wStartAgeInCpdData)) + (44.967 * log(wStartAgeInCpdData));
         } else if (gwPersonsSex == SEX_Female) {
            dUptakeAtCpdStart = -56.751 + (0.700*(wStartAgeInCpdData - gwPersonsInitAge)) -
                                (0.00163 * pow(max(79, ((wPersonsYOB + wStartAgeInCpdData) - 1900)), 2)) -
                                (3.473 * wStartAgeInCpdData) + (32.800 * sqrt(wStartAgeInCpdData));
         }
	 else {
	    throw SimException("Error", "gwPersonSex is neither SEX_male nor SEX_female");
	 }
	 
         // Calculate the Quintile Scaling factor as (cigarettes per day at age 30)/(Uptake at age 30)
         dScalingFactor = gdCigarettesPerDay[lCpdStartIndex] / dUptakeAtCpdStart;

         for (i = gwPersonsInitAge; i < wEndLoop; i++) {

            if (gwPersonsSex == SEX_Male) {
               dUptake = -38.578 + (3.342 * (sqrt(i - gwPersonsInitAge))) -
                         (0.00168 * pow(max(79, ((wPersonsYOB + i) - 1900)), 2)) -
                         (17.538 * sqrt(i)) + (44.967 * log(i));

            } else if (gwPersonsSex == SEX_Female) {
               dUptake = -56.751 + (0.700 * (i - gwPersonsInitAge)) -
                         (0.00163 * pow(max(79, ((wPersonsYOB + i) - 1900)), 2)) -
                         (3.473 * i) + (32.800 * sqrt(i));
            }
	    else {
	       throw SimException("Error", "gwPersonSex is neither SEX_male nor SEX_female");
	    }
            // In the younger ages, the uptake formula might return a negative value, use 0.10 in these cases
            if (dUptake < 0) {
               dUptake = 0.10;
            }

            gdPersonsCPDbyAge[i - gwPersonsInitAge] = dScalingFactor * dUptake;
            dSumOfCpd += gdPersonsCPDbyAge[i - gwPersonsInitAge];
         }
      }

      // If the persons started smoking before age 30, fill in the cig per day for ages 30+ (if they didn't quit before 30
      // Other wise if they started smoking after age 30, fill in the cigarettes per day array starting at that age.
      if (gwPersonsInitAge <= wStartAgeInCpdData) {
         wLookupStartAge = wStartAgeInCpdData;
      } else {
         wLookupStartAge = gwPersonsInitAge;
      }

      // Fill in the Cigarettes per day for ages 30+ directly from the cpd table
      for ( i = wLookupStartAge; i < (gwPersonsInitAge + wYearsAsSmoker); i++ ) {
         lCurrCpdIndex = lCpdStartIndex + ((i - wStartAgeInCpdData)*glCpdAgeOffset);
         if (gdCigarettesPerDay[lCurrCpdIndex] >= 0) {
            gdPersonsCPDbyAge[i - gwPersonsInitAge] = gdCigarettesPerDay[lCurrCpdIndex];
         } else {
            //This is in case the persons age goes past the max cpd for the birth cohort
            gdPersonsCPDbyAge[i - gwPersonsInitAge] = gdPersonsCPDbyAge[(i - 1) - gwPersonsInitAge];
         }
         dSumOfCpd += gdPersonsCPDbyAge[i - gwPersonsInitAge];
      }

      // Calculate average cigarettes smoked per day for the individual  
      gdPersonsAvgCPD = dSumOfCpd / (double)wYearsAsSmoker;

   } catch(SimException ex) {
      ex.AddCallPath("CalcCigarettesPerDay()");
      throw ex;
   }
}


// Switching algorithm documentation
void Smoking_Simulator::CalcCigarettesPerDaySwitch() {

   // char     sErrorMessage[500];
   short    // wIntensityLookupAge,  // Age to look up in the smoking intensity groups
            // wIntensityIndex,      // Index to start at for look up of the smoking intensity groups
            wYearsAsSmoker,       // Number of years in which person was a smoker.
            // wStartAgeInCpdData,   // The first age that has Cigarettes per day data in the table for the person's year of birth
            // wEndLoop,             // Age at which to end the uptake formula calculation
            // wLookupStartAge,      // Age to start at when getting the cigarettes per day directly from the data array
            // wPersonsYOB,          // Copy of gwPersonsYOB, when the year of birth is less than 1900, 1900 is used in the equation
            i, j, l, m, n,
            nRows,
            // finalAge,
            nColumns;
   short    group=0; 	    // A.G.: If left uninitialized, compiler complains it may be used uninitialized
	   


   long     lCpdStartIndex       // Index to start at for look up of cigarettes per day
            // ,lCurrCpdIndex        // Current index in cigarettes per day array
            ;
   double   // dIntensityProb,       // Probability to find in the lookup tables
            // dCpsForStartAge,      // The cigarettes per day for first age (in birth cohort) that has Cigarettes per day data
            // dUptake,              // Uptake formula results for persons current age
            // dUptakeAtCpdStart,    // Uptake formula results at age 30
            // dScalingFactor,       // (Cigarettes per day at age 30) / (Uptake formula at age 30)
            dSumOfCpd = 0,        // Sum of the annual cigarettes per day value (used to get average)
            roll;

   // bool     bValueFound;

   long     nValues = glCpdYOBOffset;
   nColumns = gwNumSmokingGrps;
   nRows = nValues / nColumns;


   long     cpdGroupOverLife[nRows];
   double   filteredCPDGroups[nValues],
            // filteredCPDGroupsCumSum[nValues], 
            // pSwitchCPDGroups[(nRows - 1) * nColumns],
            // pSwitchCPDGroupsCumSum[(nRows - 1) * nColumns],
            Tij[nColumns * nColumns],
            r0[nColumns],
            r1[nColumns];

   try {

      /*
      if (gdCigarettesPerDay == 0 || gdIntensityProbs == 0 || gpIndivRndsPRNG == 0)
         throw SimException("Error", "One or more of the data components for cigarettes \nper day calculation has not been initialized.\n");
      */

      if (gwPersonsInitAge == -999)
         throw SimException("Error", "CalcCigarettesPerDay should not be called for \nindividuals that do not initiate smoking.\n");

      // Using the offset formula...
      lCpdStartIndex = (glCpdRaceOffset * (gwPersonsRace)) +
                       (glCpdSexOffset * (gwPersonsSex)) +
                       (glCpdYOBOffset * GetYOBCohortGroup(gwPersonsYOB));

      // "Filter" the gdCigarettesPerDay array based on race, gender, and cohort
      for (i = 0; i < nRows; i++) {
         for (j = 0; j < nColumns; j++) {
            filteredCPDGroups[i * nColumns + j] = gdCigarettesPerDay[lCpdStartIndex + i * nColumns + j];
         }
      }

      // Determine number of years as a smoker
      if (gwPersonsCessAge == -999)
         wYearsAsSmoker = wSIM_CUTOFF_YEAR - (gwPersonsYOB + gwPersonsInitAge) + 1;
      else
         wYearsAsSmoker = gwPersonsCessAge - gwPersonsInitAge + 1;

      // Set up the array for storing the number of cigarettes smoked per day for ages 0 - 99 regardless?
      for (i = 0; i < nRows; i++) {
         cpdGroupOverLife[i] = -999;
      }

      double term1, term2;
      double switchProbs[nColumns];
      double sum;

      for (i = 0; i < nRows ; i++) {

         if (i < gwPersonsInitAge) {               // if not a smoker yet
            group = -999;

         } else if (i == gwPersonsInitAge) {       // if just inititiating
            sum = 0;
            for (j = 0; j < nColumns; j++) {
               sum += filteredCPDGroups[(i) * nColumns + j];
               switchProbs[j] = sum;
            }
            roll = GetNextRandForIndiv();
            group = 0;
            for (j = 0; j < nColumns; j++) {
               if (roll > switchProbs[j]) {
                  group += 1;
               }
            }

         } else if (i > gwPersonsInitAge) {        // if already a smoker

            // (Re)initialize Tij = 0
            for (m = 0; m < nColumns; m++) {
               for (n = 0; n < nColumns; n++) {
                  Tij[m * nColumns + n] = 0;
               }
            }

            // Find source and target proportionality vectors for the age of interest
            for (j = 0; j < nColumns; j++) {
               r0[j] = filteredCPDGroups[(i - 1) * nColumns + j];
            }

            for (j = 0; j < nColumns; j++) {
               r1[j] = filteredCPDGroups[(i - 0) * nColumns + j];
            }

            // Fill out Tij
            for (m = 0; m < nColumns; m++) {
               for (n = 0; n < nColumns; n++) {
                  term1 = r0[m];
                  term2 = r1[n];
                  for (l = 0; l < m; l++) {
                     term2 -= Tij[l * nColumns + n];
                  }
                  for (l = 0; l < n; l++) {
                     term1 -= Tij[m * nColumns + l];
                  }
                  if (min(term1, term2) < 0) 
                     Tij[m * nColumns + n] = 0;
                  else
                     Tij[m * nColumns + n] = min(term1, term2);
               }
            }

            // Normalize Tij to obtain distribution from which one can sample
            for (j = 0; j < nColumns; j++) {
               switchProbs[j] = Tij[group * nColumns + j] / r0[group];
            }

            double sumProbs;
            sumProbs = 0;
            for (m = 0; m < nColumns; m++) {
               sumProbs += switchProbs[m];
            }

            // Do the cumulative sum across them
            sum = 0;
            for (j = 0; j < nColumns; j++) {
               sum += switchProbs[j];
               switchProbs[j] = sum;
            }

            // Pull from the distribution
            roll = GetNextRandForIndiv();
            group = 0;
            for (j = 0; j < nColumns; j++) {
               if (roll > switchProbs[j])
                  group += 1;
            }
         } 

         cpdGroupOverLife[i] = group;
      }

      // Convert to cigarettees per day rather than category
      // Record the new CPD by age vector as global
      gdPersonsCPDbyAge = new double[wYearsAsSmoker];
      for (i = 0; i < wYearsAsSmoker; i++) {
         gdPersonsCPDbyAge[i] = -10;
      }
      short m, endAge;

      dSumOfCpd = 0;
      if (gwPersonsCessAge == -999)
         endAge = 99;
      else
         endAge = gwPersonsCessAge;

      for (i = gwPersonsInitAge; i <= endAge; i++) {
         m = i - gwPersonsInitAge;
         gdPersonsCPDbyAge[m] = cpdGroupOverLife[i];
         if (gdPersonsCPDbyAge[m] == 5) {
           gdPersonsCPDbyAge[m] = 60;
         } else if (gdPersonsCPDbyAge[m] == 4) {
           gdPersonsCPDbyAge[m] = 40;
         } else if (gdPersonsCPDbyAge[m] == 3) {
           gdPersonsCPDbyAge[m] = 30;
         } else if (gdPersonsCPDbyAge[m] == 2) {
           gdPersonsCPDbyAge[m] = 20;
         } else if (gdPersonsCPDbyAge[m] == 1) {
           gdPersonsCPDbyAge[m] = 10;
         } else if (gdPersonsCPDbyAge[m] == 0) {
           gdPersonsCPDbyAge[m] = 3;
         }
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
   delete [] gdInitiationProbs;    gdInitiationProbs    = 0;
   delete [] gdCessationProbs;     gdCessationProbs     = 0;
   delete [] gdLifeTableProbs;     gdLifeTableProbs     = 0;
   delete [] gdIntensityProbs;     gdIntensityProbs     = 0;
   delete [] gdCigarettesPerDay;   gdCigarettesPerDay   = 0;
   delete [] gwYOBCohortStartYrs;  gwYOBCohortStartYrs  = 0;
   delete [] gwYOBCohortEndYrs;    gwYOBCohortEndYrs    = 0;
   delete [] gdPersonsCPDbyAge;    gdPersonsCPDbyAge    = 0;
   delete gpInitiationPRNG;        gpInitiationPRNG     = 0;
   delete gpCessationPRNG;         gpCessationPRNG      = 0;
   delete gpLifeTablePRNG;         gpLifeTablePRNG      = 0;
   delete gpIndivRndsPRNG;         gpIndivRndsPRNG      = 0;
}

// Get the age at death from a cause of death other than lung cancer.
// Probability is based on the individuals smoking status and their smoking intensity (for current and former smokers)
short Smoking_Simulator::GetAgeOfDeathFromOtherCOD(short wStartAge, short wEndAge,
                                                   SmokingStatus eStatus, bool &bWentPastData) {

   short  wCurrentAge,
          wReturnAge = -999;
   bool   bPersonAlive = true;
   long   lLifeTableOffset,
          lLifeTableLocation;
   double dCurrLifeTabRand,
          dCurrLifeTabProb,
          dExcessRisk;
   char   sErrorMessage[300];

   try {
      bWentPastData = false;
      lLifeTableOffset  = (long(gwPersonsRace) * glLifeTabRaceOffset) +
                          (long(gwPersonsSex) * glLifeTabSexOffset) +
                          (long(gwPersonsYOB - GetMinYearOfBirth()) * glLifeTabYOBOffset);

      for (wCurrentAge = wStartAge; wCurrentAge < wEndAge && bPersonAlive && !bWentPastData; wCurrentAge++) {

         lLifeTableLocation = (long(wCurrentAge-gwMinLifeTableAge)*glLifeTabAgeOffset) + lLifeTableOffset;
         dCurrLifeTabRand = GetNextLifeTabRand(); //Get random value from 0 to 1 range.

         switch (eStatus) {

            case SMKST_Never:
               // Person has not initiated, get prob of dying from other COD for person who has never smoked
               dCurrLifeTabProb = gdLifeTableProbs[lLifeTableLocation + COL_Never]; break;

            case SMKST_Current:
               // Person is a current smoker, get their other COD prob based on their smoking status
               dCurrLifeTabProb = gdLifeTableProbs[lLifeTableLocation + ((int)gwPersonsSmkIntensity + 1)]; break;

            case SMKST_Former:
               // Use Excess Risk for Former Smokers formula (Davis Burns et al.)
               // New in Version 3.0, program now uses the average cigarettes smoked per day for a person.
               dExcessRisk = exp((B0 + B1 * gdPersonsAvgCPD + B2 * gwPersonsCessAge) * pow((wCurrentAge - gwPersonsCessAge), B3));
               // Multiply Excessive risk by difference between Current (for their smoking intenity) and Never probability
               // then add that result to the Never Probability to get the Probability the Person will die that year
               dCurrLifeTabProb = gdLifeTableProbs[lLifeTableLocation + COL_Never] +
                                  ((gdLifeTableProbs[lLifeTableLocation + ((int)gwPersonsSmkIntensity + 1)] -
                                    gdLifeTableProbs[lLifeTableLocation + COL_Never])
                                    * dExcessRisk); break;

            default:
               sprintf(sErrorMessage, "Invalid Smoking Status: %d.\n", eStatus);
               throw SimException("Error", sErrorMessage);
         }

         if ( dCurrLifeTabRand <= dCurrLifeTabProb ) {
            bPersonAlive = false;
            wReturnAge = wCurrentAge;
         }

         // If the probability was missing, it was coded as -1, life table 
         // checking can stop once a -1 is reached
         if (dCurrLifeTabProb < 0) {
             bWentPastData = true;
         }
      }

   } catch (SimException ex) {
      ex.AddCallPath("GetAgeOfDeathFromOtherCOD(short, short, enum, bool)");
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

double Smoking_Simulator::GetNextCessRand() {
   double dReturnValue;
   if (gpCessationPRNG == NULL)
      throw SimException("GetNextCessRand()", 
         "Call to PRNG before PRNG has been initialized with a seed.");
   dReturnValue = gpCessationPRNG->genrand_real1();
   return dReturnValue;
}

double Smoking_Simulator::GetNextInitRand() {
   double dReturnValue;
   if (gpInitiationPRNG == NULL)
      throw SimException("GetNextInitRand()", "Call to PRNG before PRNG has been initialized with a seed.");
   dReturnValue = gpInitiationPRNG->genrand_real1();
   return dReturnValue;
}

double Smoking_Simulator::GetNextLifeTabRand() {
   double dReturnValue;
   if (gpLifeTablePRNG == NULL)
      throw SimException("GetNextLifeTabRand()", "Call to PRNG before PRNG has been initialized with a seed.");
   dReturnValue = gpLifeTablePRNG->genrand_real1();
   return dReturnValue;
}

double Smoking_Simulator::GetNextRandForIndiv() {
   double dReturnValue;
   if (gpIndivRndsPRNG == NULL)
      throw SimException("GetNextRandForIndiv()", "Call to PRNG before PRNG has been initialized with a seed.");
   dReturnValue = gpIndivRndsPRNG->genrand_real1();
   return dReturnValue;
}

// Get the birth cohort group that the year of birth corresponds to.
short Smoking_Simulator::GetYOBCohortGroup(short wYearBirth) {

   short wReturnValue         = -1,
         wSearchLow           = 0,
         wSearchMid,
         wSearchHigh;
   char  sErrorMessage[500];
   bool  bValueFound          = false;

   if (wYearBirth < gwYOBCohortStartYrs[0]) {
      sprintf( sErrorMessage, "Year of Birth - %d is less than the minimum year of birth allowed - %d", \
         wYearBirth, gwYOBCohortStartYrs[0] );
      throw SimException("GetYOBCohortGroup(short)", sErrorMessage);
   }

   if ( wYearBirth > gwYOBCohortEndYrs[gwNumBirthCohorts - 1] ) {
      sprintf(sErrorMessage, "Year of Birth - %d is greater than the maximum year of birth allowed - %d", \
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

// Initialize the private variables, set pointers to zero
void Smoking_Simulator::Init() {
   gwNumSexValues       = 0;
   gwNumRaceValues      = 0;
   gwNumBirthCohorts    = 0;

   //Set pointers to zero
   gpInitiationPRNG     = 0;
   gpCessationPRNG      = 0;
   gpLifeTablePRNG      = 0;
   gpIndivRndsPRNG      = 0;
   gdInitiationProbs    = 0;
   gdCessationProbs     = 0;
   gdLifeTableProbs     = 0;
   gdIntensityProbs     = 0;
   gdCigarettesPerDay   = 0;
   gwYOBCohortStartYrs  = 0;
   gwYOBCohortEndYrs    = 0;
   gdPersonsCPDbyAge    = 0;

   geOutputType         = OUT_DataOnly;

   gbImmediateCessation = false;
   gwImmediateCessYear  = 0;
}


void Smoking_Simulator::InitPRNGs(unsigned long ulInitSeed,    unsigned long ulCessSeed,
                                  unsigned long ulLifeTabSeed, unsigned long ulIndRndsSeed) {

   if (gpInitiationPRNG != NULL)
      throw SimException("InitPRNGs()", "Initiation PRNG is already initialized.\n");
   if (gpCessationPRNG != NULL)
      throw SimException("InitPRNGs()", "Cessation PRNG is already initialized.\n");
   if (gpLifeTablePRNG != NULL)
      throw SimException("InitPRNGs()","Life Table PRNG is already initialized.\n");
   if (gpIndivRndsPRNG != NULL) {
      throw SimException("InitPRNGs()","The PRNG that generates random numbers for the \nindividual \
         person is already initialized.\n");
   }

   gpInitiationPRNG = new MersenneTwister(ulInitSeed);
   gpCessationPRNG  = new MersenneTwister(ulCessSeed);
   gpLifeTablePRNG  = new MersenneTwister(ulLifeTabSeed);
   gpIndivRndsPRNG  = new MersenneTwister(ulIndRndsSeed);
}

// Read in the cigarettes per day data file, this function assumes the data
// is sorted by race, sex , YOB cohort, age and intensity group
// The data will be stored in an array that is offset by race, sex, year of birth
// age and smoking intensity level
void Smoking_Simulator::LoadCPDFile(const char* sCpdFile) {

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

      /*
      if (gdIntensityProbs == NULL)
         throw SimException("Error", "The smoking intensity probability file must be loaded before the Cigarettes per day data file.\n");
      */

      pCpdFile = fopen(sCpdFile, "r");
      if (pCpdFile == NULL) {
	      sprintf(sErrorMessage, "The specified input file '%s' does not exist\n or could not be opened.\n\n", sCpdFile);
	      throw SimException("Error", sErrorMessage);
	   }

	   // Read in the first line of the file. Line contains the line number where the data in the file begins
      // This is to allow documentation to be placed in the input file
	   fgets(sInputLine, 3000, pCpdFile);
	   if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sCpdFile);
	      throw SimException("Error", sErrorMessage);
	   }

	   pTokenPtr = strtok(sInputLine, ",");
      wFirstDataLine = atoi(pTokenPtr);

      if (wFirstDataLine <= 1) {
	      sprintf(sErrorMessage, "Invalid value: %ld for location of first data line read in from file %s", \
            wFirstDataLine, sCpdFile);
	      throw SimException("Error", sErrorMessage);
      }

      // Read in the Documentation lines, If the tag Version= is found, store it in the Version Num string for the file
      for (i = 2; i < wFirstDataLine; i++) {
         if ( fgets(sInputLine, 3000, pCpdFile) == NULL) {
     	      sprintf(sErrorMessage, "Error in  file %s, End of File reached before location of first data line \
               as specified in line 1\n", sCpdFile);
   	      throw SimException("Error", sErrorMessage);
         }
      }

      // Read in the First data line which contains the # of race values, # of sex values,
      // # of birth cohort group values, the minimum age in the data, the maximum age age in the data
      // and the number of smoking intensity groups, in the order they are listed here.
      fgets(sInputLine, 3000, pCpdFile);

      if (sInputLine == NULL) {
	      sprintf(sErrorMessage,"Error reading first DATA line of file %s", sCpdFile);
	      throw SimException("Error", sErrorMessage);
	   }

      pTokenPtr       = strtok(sInputLine, ",");
      wRaceValue      = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
      wSexValue       = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
      wNumCohorts     = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
      wMinAgeValue    = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
      wMaxAgeValue    = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
      wNumSmokingGrps = atoi(pTokenPtr);

      gwNumSmokingGrps = wNumSmokingGrps;

      /*
      if ((wRaceValue != gwNumRaceValues) || (wSexValue != gwNumSexValues) || (wNumCohorts != gwNumBirthCohorts)) {
         sprintf(sErrorMessage, "Mismatch between values defined from Initiation Prob Data file and this file.\n\
            Race: Init = %d, CPD = %d\nSex: Init = %d, CPD = %d\nNum Cohorts: Init = %d, CPD = %d\n", gwNumRaceValues, \
            wRaceValue, gwNumSexValues, wSexValue, gwNumBirthCohorts, wNumCohorts);
	      throw SimException("Error", sErrorMessage);
      }

      if (wNumSmokingGrps != gwNumIntensityGrps) {
         sprintf(sErrorMessage, "Mismatch between the number of smoking intensity groups defined in the Intensity \
            Prob Data file and this file.\nIntensity file has %d groups, this file indicates %d groups.\n", gwNumIntensityGrps, \
            wNumSmokingGrps);
	      throw SimException("Error", sErrorMessage);
      }
      */

      if (wMinAgeValue < 0 || wMaxAgeValue <= 0 || wMinAgeValue >=  wMaxAgeValue) {
	      sprintf(sErrorMessage,"Invalid value(s) for minimum and maximum initiation ages\n read in from file %s",sCpdFile);
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

      while (fgets(sInputLine, 3000, pCpdFile) != NULL) {

         lNumLinesRead++;

         pTokenPtr          = strtok(sInputLine, ",");
         wRaceValue         = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wSexValue          = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wCohortStartValue  = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wCohortEndValue    = atoi(pTokenPtr);     pTokenPtr = strtok(NULL, ",");
         wAgeValue          = atoi(pTokenPtr);

         wCurrCohort        = GetYOBCohortGroup(wCohortStartValue);

         if (wCohortStartValue != gwYOBCohortStartYrs[wCurrCohort] ||
             wCohortEndValue != gwYOBCohortEndYrs[wCurrCohort]) {
            sprintf(sErrorMessage, "The cohort range %ld - %ld in the Cigarettes per day file does not match the cohort \
               range set by the initiation file.\n", wCohortStartValue, wCohortEndValue);
            throw SimException("Error", sErrorMessage);
         }

         // Validate values read in
         if (wAgeValue  < gwCpdMinAge  || wAgeValue > gwCpdMaxAge ||
             wRaceValue >= gwNumRaceValues || wRaceValue < 0 ||
             wSexValue  >= gwNumSexValues || wSexValue  < 0) {
            sprintf(sErrorMessage, "Invalid By-Variable Combination, Race = %ld, Sex = %ld, Age = %ld\n Read form file %s \
               at line number %ld", wRaceValue, wSexValue, wAgeValue, sCpdFile, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }

         // Probabilities are read in by smoking intesity group
         // Value assignment within the array is based on the offset formula
         // fprintf(stdout, "%d\n", lNumLinesRead);
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
      }

      if (lNumLinesRead > lMaxLinesExpected) {
         sprintf(sErrorMessage, "Too many lines (%ld) read from file %s.\n%ld were expected based on sex, race, birth cohort and \
            age values specified in first line of file.", lNumLinesRead, sCpdFile, lMaxLinesExpected);
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
	      sprintf(sErrorMessage, "The specified input file '%s' does not exist\n or could not be opened.\n\n", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }

	   // Read in the first line of the file.  Line contains the line number where the data in the file begins
      // This is to allow documentation to be placed in the input file
	   fgets(sInputLine, 3000, pProbabilityFile);
	   if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }
	   pTokenPtr = strtok(sInputLine, ",");
      wFirstDataLine = atoi(pTokenPtr);
      if (wFirstDataLine <= 1) {
	      sprintf(sErrorMessage,  "Invalid value: %d for location of first data line read in from file %s", \
            wFirstDataLine, sDataFileName);
	      throw SimException("Error", sErrorMessage);
      }

      // Read in the Documentation lines, If the tag Version= is found, store it in the Version Num string for the file
      for (i = 2; i < wFirstDataLine; i++) {
         if ( fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
     	      sprintf(sErrorMessage, "Error in  file %s, End of File reached before location of first data line as \
               specified in line 1\n", sDataFileName);
   	      throw SimException("Error", sErrorMessage);
         }
      }

      // Read in the First data line which contains the # of race values, # of sex values,
      // # of birth cohort group values, the minimum inititaion age and the maximum initiation age
      // in the order they are listed here.
      fgets(sInputLine, 3000, pProbabilityFile);

      if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sDataFileName);
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
	      sprintf(sErrorMessage, "Invalid value(s) for minimum and maximum initiation ages\n read in from file %s", \
            sDataFileName);
         throw SimException("Error", sErrorMessage);
      }

      if ((wNumRaces != gwNumRaceValues) || (wNumSexes != gwNumSexValues)) {
         sprintf(sErrorMessage, "Mismatch between number of races and number of sexes in initiation file and cohorts from CPD Intensity \
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
            sprintf(sErrorMessage, "Invalid Race Value: %d\n Read from file %s at line number %ld", wRaceValue, 
               sDataFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }
         if (wSexValue > wNumSexes) {
            sprintf(sErrorMessage, "Invalid Race Value: %d\n Read from file %s at line number %ld", wSexValue, \
               sDataFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }
         if (wAgeValue < gwIntensityMinAge || wAgeValue > gwIntensityMaxAge) {
            sprintf(sErrorMessage, "Invalid Age Value: %d\n Read from file %s at line number %ld", wAgeValue, \
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
                  sprintf(sErrorMessage, "Invalid Probability: %f read for Age : %d ,Intensity Group : %d\nRead \
                     from file %s at line number %ld.\n", dCurrProbability, wAgeValue, i, sDataFileName, lNumLinesRead);
                  throw SimException("Error",sErrorMessage);
               }
            } else {
               sprintf(sErrorMessage, "Value missing for Age : %d ,Intensity Group : %d\nValue must contain a decimal palce.\n", wAgeValue,i);
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
         sprintf(sErrorMessage, "Not enough lines (%ld) read from file %s.\n%ld were expected based on sex, race, birth cohort \
            and age values specified in first line of file.", lNumLinesRead, sDataFileName, lNumLinesExpected);
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


   try {

      if ((eFileType != DATA_Initiation) && (eFileType != DATA_Cessation)) 
         throw SimException("Error", "Invalid File Type supplied to function.");

      if (eFileType == DATA_Cessation && (gdInitiationProbs==NULL)) {
         throw SimException("Error", 
            "Attempt to load Cessation Probabilities before Initiation probabilities.\nInitiation data must be loaded first.\n");
      }

      pProbabilityFile = fopen(sDataFileName, "r");
      if (pProbabilityFile == NULL) {
	      sprintf(sErrorMessage,"The specified input file '%s' does not exist\n or could not be opened.\n\n", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }

	   //Read in the first line of the file. Line contains the line number where the data in the file begins
      // This allows documentation to be placed in the input file
	   fgets(sInputLine, 3000, pProbabilityFile);
	   if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }

	   pTokenPtr = strtok(sInputLine, ",");
      wFirstDataLine = atoi(pTokenPtr);
      if (wFirstDataLine <= 1) {
	      sprintf(sErrorMessage, "Invalid value: %d for location of first data line read in from file %s", \
            wFirstDataLine, sDataFileName);
	      throw SimException("Error", sErrorMessage);
      }

      // Read in the Documentation lines, If the tag Version= is found, store it in the Version Num string for the file
      for (i = 2; i < wFirstDataLine; i++) {
         if ( fgets(sInputLine, 3000, pProbabilityFile) == NULL) {
     	      sprintf(sErrorMessage, "Error in  file %s, End of File reached before location of first data line \
               as specified in line 1\n", sDataFileName);
   	      throw SimException("Error", sErrorMessage);
         }
      }

      // Read in the First data line which contains the # of race values, # of sex values,
      // # of birth cohort group values, the minimum inititaion age and the maximum initiation age
      // in the order they are listed here.
      fgets(sInputLine, 3000, pProbabilityFile);

      if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sDataFileName);
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

      if ((eFileType == DATA_Initiation) && (wRaceValue <= 0 || wSexValue <= 0 || wCohortValue <= 0))
         throw SimException("Error", "Invalid value read in for # of sex values, # of race values or # of birth cohorts.");

      if ((eFileType == DATA_Cessation) &&
         ((wRaceValue != gwNumRaceValues) || (wSexValue != gwNumSexValues) || (wCohortValue != gwNumBirthCohorts))) {
         sprintf(sErrorMessage, "Mismatch between cohort values from Initiation and Cessation Files.\n\
            Race: Init = %d, Cess = %d\nSex: Init = %d, Cess = %d\nNum Cohorts: Init = %d, Cess = %d\n", \
            gwNumRaceValues, wRaceValue, gwNumSexValues, wSexValue, gwNumBirthCohorts, wCohortValue);
	      throw SimException("Error", sErrorMessage);
      }

      if (wMinAgeValue < 0 || wMaxAgeValue <= 0 || wMinAgeValue >=  wMaxAgeValue) {
	      sprintf(sErrorMessage, "Invalid value(s) for minimum and maximum initiation ages\n read in from file %s", sDataFileName);
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
         lNumLinesExpected    = long(gwNumSexValues * gwNumRaceValues *
                                   ((gwMaxCessationAge - gwMinCessationAge) + 1));
      }

      // Read in the second dataline, this contains 3 column labels followed by the YOB cohort ranges
      fgets(sInputLine, 3000, pProbabilityFile);

      if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading second DATA line of file %s", sDataFileName);
	      throw SimException("Error", sErrorMessage);
	   }

      pTokenPtr= strtok(sInputLine, ",");
      pTokenPtr= strtok(NULL, ",");
      pTokenPtr= strtok(NULL, ",");

      // // Read in the year of birth cohorts and assign the values to the appropriate Start/End year arrays
      for (i = 0; i < gwNumBirthCohorts; i++) {

         pTokenPtr     = strtok(NULL, "-");
         wCohortValue  = atoi(pTokenPtr);

         // If it's the initiation file, assign value to the array
         if (eFileType == DATA_Initiation) {
            gwYOBCohortStartYrs[i] = wCohortValue;

         // Otherwise check the value against the value already in the array
         } else if (wCohortValue != gwYOBCohortStartYrs[i]) {
            sprintf(sErrorMessage, "Mismatching starting cohorts between Initiation and Cessation probability \
               files\nFor range : 1\n%d read from initiation file.\n%d read from cessation file.", 
               gwYOBCohortStartYrs[i], wCohortValue);
            throw SimException("Error", sErrorMessage);
         }

         pTokenPtr = strtok(NULL, ",");
         wCohortValue = atoi(pTokenPtr);

         if (eFileType == DATA_Initiation)  {
            // If its the Initiation file, assign value to the array
            gwYOBCohortEndYrs[i] = wCohortValue;
         } else if (wCohortValue != gwYOBCohortEndYrs[i]) {
            // Otherwise check the value against the value already in the array
            sprintf(sErrorMessage, "Mismatching starting cohorts between Initiation and Cessation probability files\n\
               For range : 1\n%d read from initiation file.\n%d read from cessation file.", gwYOBCohortEndYrs[i], wCohortValue);
            throw SimException("Error", sErrorMessage);
         }

         // If its the initiation file, verify the values
         if ( (eFileType == DATA_Initiation) &&
              (gwYOBCohortStartYrs[i] < 0 || gwYOBCohortEndYrs[i] <= 0 || gwYOBCohortStartYrs[i] > gwYOBCohortEndYrs[i]) ) {
            sprintf(sErrorMessage, \
              "Invalid Year of Birth Cohort value(s).\nStart Year = %d, End Year = %d.\nRead in from file %s for cohort range: %d\n\n\n", \
              gwYOBCohortStartYrs[i], gwYOBCohortEndYrs[i], sDataFileName, i);

            throw SimException("Error", sErrorMessage);
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
            sprintf(sErrorMessage, "Invalid By-Variable Combination, Race = %d, Sex = %d, Age = %d\n Read form file %s at line \
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
                  sprintf(sErrorMessage, "Invalid Probability: %f read for Birth Cohort: %d - %d\nRead from file %s at line \
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
         sprintf(sErrorMessage,"Not enough lines (%ld) read from file %s.\n%ld were expected based on sex, race, birth cohort and age values \
            specified in first line of file.", lNumLinesRead, sDataFileName, lNumLinesExpected);
         throw SimException("Error", sErrorMessage);
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

// Load the probability initiation/cessation data files.
void Smoking_Simulator::LoadOtherCODFile(const char* sLifeTableFileName) {

   char     sInputLine[1001],
            sErrorMessage[500],
           *pTokenPtr            = 0;
   long     lMaxNumLines,
            lNumLinesRead,
            lCurrArrayLocation,
            lSizeOfLifeTable,
            j;
   double   dCurrProbability;
   short    wFirstDataLine,
            wSexValue,
            wRaceValue,
            wYearValue,
            wAgeValue,
            i;
   FILE    *pLifeTableFile     = 0;

   try {
      if (gdInitiationProbs == NULL)
         throw SimException("Error", "Initiation Probabilies must be loaded before the Life Table Probabilities.\n");

      pLifeTableFile = fopen(sLifeTableFileName, "r");
      if (pLifeTableFile == NULL) {
	      sprintf(sErrorMessage, "The specified input file '%s' does not exist\n or could not be opened.\n\n", sLifeTableFileName);
	      throw SimException("Error", sErrorMessage);
	   }

	   // Read in the first line of the file. Line contains the line number where the data in the file begins
      // This is to allow documentation to be placed in the input file
	   fgets(sInputLine, 3000, pLifeTableFile);

	   if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sLifeTableFileName);
	      throw SimException("Error", sErrorMessage);
	   }

	   pTokenPtr      = strtok(sInputLine, ",");
      wFirstDataLine = atoi(pTokenPtr);
      if (wFirstDataLine <= 1) {
	      sprintf(sErrorMessage, "Invalid value: %d for location of first data line to read in from file %s", \
            wFirstDataLine, sLifeTableFileName);
	      throw SimException("Error", sErrorMessage);
      }

      // Read in the Documentation lines, If the tag Version= is found, store it in the Version Num string for the file
      for (i = 2; i < wFirstDataLine; i++) {
         if ( fgets(sInputLine, 3000, pLifeTableFile) == NULL) {
   	      sprintf(sErrorMessage, "Error in  file %s, End of File reached before location of first data line as specified in line 1\n", \
               sLifeTableFileName);
   	      throw SimException("Error", sErrorMessage);
         }
      }

      // Read in the First data line which contains the # of race values, # of sex values,
      // the min year of birth, the max year of birth, the min age and the maximum age
      // in the order they are listed here.
      fgets(sInputLine, 3000, pLifeTableFile);

      if (sInputLine == NULL) {
	      sprintf(sErrorMessage, "Error reading first DATA line of file %s", sLifeTableFileName);
	      throw SimException("Error", sErrorMessage);
	   }

      pTokenPtr          = strtok(sInputLine, ",");
      wRaceValue         = atoi(pTokenPtr);
      pTokenPtr          = strtok(NULL, ",");
      wSexValue          = atoi(pTokenPtr);
      pTokenPtr          = strtok(NULL, ",");
      gwMinLifeTableYear = atoi(pTokenPtr);
      pTokenPtr          = strtok(NULL, ",");
      gwMaxLifeTableYear = atoi(pTokenPtr);
      pTokenPtr          = strtok(NULL, ",");
      gwMinLifeTableAge  = atoi(pTokenPtr);
      pTokenPtr          = strtok(NULL, ",");
      gwMaxLifeTableAge  = atoi(pTokenPtr);

      gwMaxLifeTableYear = 2300;

      /*
      if ((wRaceValue != gwNumRaceValues) || (wSexValue != gwNumSexValues) ||
         (gwMinLifeTableYear > GetMinYearOfBirth()) || (gwMaxLifeTableYear < GetMaxYearOfBirth())) {
         sprintf(sErrorMessage, "Mismatch between cohort values from Life Table file and cohorts from Initiation file.\
            \nRace: Init = %d, Life = %d\nSex: Init = %d, Life = %d\nMin Year Birth: Init = %d, Life = %d\nMax Year Birth: \
            Init = %d, Life = %d\n", gwNumRaceValues, wRaceValue, gwNumSexValues, wSexValue, GetMinYearOfBirth(), \
            gwMinLifeTableYear, GetMaxYearOfBirth(), gwMaxLifeTableYear);
	      throw SimException("Error", sErrorMessage);
      }
      */

      if (gwMinLifeTableAge < 0 || gwMaxLifeTableAge <= 0 || gwMinLifeTableAge >=  gwMaxLifeTableAge) {
	      sprintf(sErrorMessage, "Invalid value(s) for minimum and maximum initiation ages\n read in from file %s", sLifeTableFileName);
         throw SimException("Error", sErrorMessage);
      }

      // Load private members from Life Table data
      glLifeTabAgeOffset  = long(COL_NumColumns);
      glLifeTabYOBOffset  = long(((gwMaxLifeTableAge - gwMinLifeTableAge) + 1) * glLifeTabAgeOffset);
      glLifeTabSexOffset  = long(((gwMaxLifeTableYear - gwMinLifeTableYear) + 1) * glLifeTabYOBOffset);
      glLifeTabRaceOffset = long(gwNumSexValues) * glLifeTabSexOffset;
      lSizeOfLifeTable    = long(gwNumRaceValues) * long(glLifeTabRaceOffset);
      gdLifeTableProbs    = new double[lSizeOfLifeTable];
      lMaxNumLines        = long(gwNumRaceValues * gwNumSexValues *
                                 ((gwMaxLifeTableYear - gwMinLifeTableYear)+1) *
                                 ((gwMaxLifeTableAge - gwMinLifeTableAge) + 1));

      // Fill in all gdLifeTableProbs entries with -1
      for (j=0; j<lSizeOfLifeTable; j++) {
         gdLifeTableProbs[j] = -1;
      }

      // Read in the Probability Data Lines
      lNumLinesRead = 0;
      while (fgets(sInputLine, 3000, pLifeTableFile)!=NULL) {

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
         if (wAgeValue  < gwMinLifeTableAge    || wAgeValue > gwMaxLifeTableAge ||
            wRaceValue >= gwNumRaceValues      || wRaceValue < 0                ||
            wSexValue  >= gwNumSexValues       || wSexValue  < 0                ||
            wYearValue > gwMaxLifeTableYear   || wYearValue < gwMinLifeTableYear) {
            sprintf(sErrorMessage, "Invalid By-Variable Combination, Race = %d, Sex = %d, Year = %d, Age = %d\n Read form file %s \
               at line number %ld", wRaceValue, wSexValue, wYearValue, wAgeValue, sLifeTableFileName, lNumLinesRead);
            throw SimException("Error", sErrorMessage);
         }

         // Probabilities are read in by smoking status type
         // Value assignment within the array is based on the offset formula
         for (i = 0; i < COL_NumColumns; i++) {
            pTokenPtr = strtok(NULL, ",");
            dCurrProbability  = atof(pTokenPtr);
            if ((dCurrProbability < 0) || (dCurrProbability > 1)) {
               sprintf(sErrorMessage, "Invalid Probability: %f read for Birth Cohort: %d - %d\nRead from file %s at line number %ld.\n", \
                  dCurrProbability, gwYOBCohortStartYrs[i], gwYOBCohortEndYrs[i], sLifeTableFileName, lNumLinesRead);
               throw SimException("Error", sErrorMessage);
            }
            lCurrArrayLocation = (long(wRaceValue) * glLifeTabRaceOffset) +
                                 (long(wSexValue) * glLifeTabSexOffset) +
                                 (long(wYearValue - gwMinLifeTableYear) * glLifeTabYOBOffset) +
                                 (long(wAgeValue - gwMinLifeTableAge) * glLifeTabAgeOffset)   +
                                  long(i);
            gdLifeTableProbs[lCurrArrayLocation] = dCurrProbability;
         }
      }

      if (lNumLinesRead > lMaxNumLines) {
         sprintf(sErrorMessage, "Too many lines read from file %s.\n%ld max were expected based on sex, race, birth cohort and age\
            values specified in first line of file.\n%ld were read in.\n", sLifeTableFileName, lMaxNumLines, lNumLinesRead);
         throw SimException("Error", sErrorMessage);
      }

      // End Reading in the Probabilities File
      fclose(pLifeTableFile);

   } catch (SimException ex) {
      if (pLifeTableFile != NULL)
         fclose(pLifeTableFile);
      ex.AddCallPath("LoadLifeTableFile()");
      throw ex;
   } catch (...) {
      if (pLifeTableFile != NULL)
         fclose(pLifeTableFile);
      throw SimException("LoadLifeTableFile()", "Unkown Error Occurred.\n");
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
         pTokenPtr= strtok(sCurrInputLine, ";");
         wRace = atoi(pTokenPtr);
         pTokenPtr= strtok(NULL, ";");
         wSex = atoi(pTokenPtr);
         pTokenPtr= strtok(NULL, ";");
         wYOB = atoi(pTokenPtr);

         RunSimulation(wRace, wSex, wYOB, pOutputFile);
         if (bPrintToScreen) 
            WriteToStream(stdout);
      }

      fclose(pInputFile);
      if (pOutputFile!=0)
         fclose(pOutputFile);

   } catch (SimException ex) {
      ex.AddCallPath("RunSimulation(char*,char*,bool)");
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
void Smoking_Simulator::RunSimulation(short wRace, short wSex, short wYearBirth, FILE* pOutStream) {

   short    wYOBCohortGroup,
            wCurrentAge          = gwMinInitiationAge,
            wAgeAtDeath;
   long     wSearchOffset;
   bool     bCanInitiate         = true,
            bForceCessation      = false,
            bPersonInitiated     = false,
            bPersonQuit          = false,
            bPassedCohortMaxAge  = false,
            bPassedLifeTabMaxAge = false;
   double   dCurrInitiationRand,
            dCurrInitiationProb,
            dCurrCessationRand,
            dCurrCessationProb;
   char     sErrorMessage[500];

   try {

      // Validate Input
      if ((wYearBirth < GetMinYearOfBirth()) || (wYearBirth > 2100)) { // GetMaxYearOfBirth())) {
         sprintf(sErrorMessage, "Invalid Year of Birth: %d, supplied to Smoking History Simulator.", wYearBirth);
         throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
      }

      if ( (wSex < 0) || (wSex >= gwNumSexValues) ) {
         sprintf(sErrorMessage, "Invalid Sex Value: %d, supplied to Smoking History Simulator.", wSex);
         throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
      }

      if ( (wRace < 0) || (wRace >= gwNumRaceValues) ) {
         sprintf(sErrorMessage, "Invalid Race Value: %d, supplied to Smoking History Simulator.", wRace);
         throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
      }

      if ( (wRace == 1) && (wSex == 1) ) {
         sprintf(sErrorMessage, "Invalid Race/Sex Combination: %d/%d, supplied to Smoking History Simulator.", wRace, wSex);
         throw SimException("Error", sErrorMessage, SimException::NON_FATAL);
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
      wSearchOffset     = ((gwPersonsRace)*gwInitProbRaceOffset) + ((gwPersonsSex)*gwInitProbSexOffset) +
                           (wYOBCohortGroup*gwInitProbYOBOffset);

      // Smoking Initiation Routine
      // 3 instances in which scanning the initiation loop stops
      // Person initiates smoking, person surpasses max initiation age for their cohort,
      // person surpasses overall max initiation age,
      while (!bPersonInitiated && !bPassedCohortMaxAge && (wCurrentAge <= gwMaxInitiationAge)) {

         // Get Initiation Probabilities
         dCurrInitiationRand = GetNextInitRand(); // Get random value from 0 to 1 range.
         dCurrInitiationProb = gdInitiationProbs[(wCurrentAge - gwMinInitiationAge) + wSearchOffset];

         // If ImmediateCessation is turned on, check if the current year (birth year + current age) 
         // is equal to or greater than the last year before cessation begins.
         if (gbImmediateCessation && ((gwPersonsYOB + wCurrentAge) >= (gwImmediateCessYear-1))) {
            bCanInitiate = false;
         }

         if (dCurrInitiationRand <= dCurrInitiationProb && bCanInitiate) {
            gwPersonsInitAge = wCurrentAge;
            bPersonInitiated = true;
         }

         // If the probability was missing, it was coded as -1, sim can 
         // stop once one of these values are reached.
         if (dCurrInitiationProb < 0 || (((wCurrentAge+1) + gwPersonsYOB) > wSIM_CUTOFF_YEAR)) {
            bPassedCohortMaxAge = true;
         }

         // Increment the age if they did not initiate
         if (!bPersonInitiated) {
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

         wSearchOffset = ((gwPersonsRace)*gwCessProbRaceOffset) + ((gwPersonsSex)*gwCessProbSexOffset) +
                          ((wYOBCohortGroup)*(gwCessProbYOBOffset));

         while (!bPersonQuit && !bPassedCohortMaxAge && (wCurrentAge <= gwMaxCessationAge)) {

            // If ImmediateCessation is turned on, check if the current year (birth year + current age) is 
            // equal to or greater than the last year before cessation begins.
            if (gbImmediateCessation && ((gwPersonsYOB + wCurrentAge) >= (gwImmediateCessYear-1))) {
               bForceCessation = true;
            }

            dCurrCessationRand = GetNextCessRand();
            dCurrCessationProb = gdCessationProbs[(wCurrentAge-gwMinCessationAge)+wSearchOffset];

            if (dCurrCessationRand <= dCurrCessationProb || bForceCessation) {
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
      if (!bPersonInitiated) {
         gwPersonsAgeAtDeath = GetAgeOfDeathFromOtherCOD(gwMinLifeTableAge, gwMaxLifeTableAge + 1, SMKST_Never, bPassedLifeTabMaxAge);

      // People who start smoking, and never quit
      } else if (bPersonInitiated && !bPersonQuit) {
         wAgeAtDeath = GetAgeOfDeathFromOtherCOD(gwMinLifeTableAge, gwPersonsInitAge, SMKST_Never, bPassedLifeTabMaxAge);
         if ( (wAgeAtDeath == -999) && !bPassedLifeTabMaxAge ) {
            wAgeAtDeath = GetAgeOfDeathFromOtherCOD(gwPersonsInitAge,gwMaxLifeTableAge+1, SMKST_Current, bPassedLifeTabMaxAge);
         }
         gwPersonsAgeAtDeath = wAgeAtDeath;

      // People who start smoking and quit smoking
      } else if (bPersonInitiated && bPersonQuit) {
         wAgeAtDeath = GetAgeOfDeathFromOtherCOD(gwMinLifeTableAge,gwPersonsInitAge, SMKST_Never, bPassedLifeTabMaxAge);
         if ((wAgeAtDeath == -999) && !bPassedLifeTabMaxAge) {
            wAgeAtDeath = GetAgeOfDeathFromOtherCOD(gwPersonsInitAge,gwPersonsCessAge, SMKST_Current, bPassedLifeTabMaxAge);
            if ((wAgeAtDeath == -999) && !bPassedLifeTabMaxAge) {
               wAgeAtDeath = GetAgeOfDeathFromOtherCOD(gwPersonsCessAge,gwMaxLifeTableAge+1, SMKST_Former, bPassedLifeTabMaxAge);
            }
         }
         gwPersonsAgeAtDeath = wAgeAtDeath;
      }

      if (pOutStream != 0)
         WriteToStream(pOutStream);

      // Oversample the PRNGs (only does the PRNG that generates Randoms for the individual)
      // More oversampling can be added if desired.
      OversamplePRNGs();

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
      sprintf(sErrorMessage, "Invalid Value supplied for Output Type : %d", wOutputType);
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

   fprintf(pOutStream, "========================================================\n");
   fprintf(pOutStream, " Race:            %s\n", sRACE_LABELS[gwPersonsRace]);
   fprintf(pOutStream, " Sex:             %s\n", sSEX_LABELS[gwPersonsSex]);
   fprintf(pOutStream, " Year Of Birth:   %d\n", gwPersonsYOB);

   if (gwPersonsInitAge >= 0) {
      fprintf(pOutStream," Initiation Age:  %d\n", gwPersonsInitAge);
      if (gwPersonsCessAge >= 0)
         fprintf(pOutStream, " Cessation Age:   %d\n", gwPersonsCessAge);
      else fprintf(pOutStream, " Cessation Age:   Person Never Quit Smoking.\n"); } else {
      fprintf(pOutStream, " Initiation Age:  Person Never Initiated Smoking.\n");
   }

   if (gwPersonsAgeAtDeath >= 0) {
      fprintf(pOutStream, " Age At Death:    %d\n", gwPersonsAgeAtDeath);
   } else {
      fprintf(pOutStream, " Age At Death:    Person alive through %d.\n", wSIM_CUTOFF_YEAR);
   }

   if (gwPersonsInitAge >= 0) {
      fprintf(pOutStream, " People are not put into a smoker category for life in SHG v2.0.");
      fprintf(pOutStream, " Intensity Probability : %f .\n", gdTempIntensityProb);

      if (gwPersonsCessAge == -999)
         wYearsAsSmoker = (wSIM_CUTOFF_YEAR - (gwPersonsYOB+gwPersonsInitAge)) + 1;
      else
         wYearsAsSmoker = (gwPersonsCessAge - gwPersonsInitAge) + 1;

      fprintf(pOutStream, " Age        Cigarettes per day\n");

      for (i=0; i<wYearsAsSmoker; i++) {
         if (i + gwPersonsInitAge < 100) {
            fprintf(pOutStream, " %d         %f\n", (i+gwPersonsInitAge), gdPersonsCPDbyAge[i]);
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

   fprintf(pOutStream, "Hist !%c %c %d ", sRACE_LABELS[gwPersonsRace][0], sSEX_LABELS[gwPersonsSex][0], gwPersonsYOB);

   if (gwPersonsInitAge >= 0 && gwPersonsCessAge >= 0)
      fprintf(pOutStream, "%d %d ", gwPersonsInitAge, gwPersonsCessAge);
   else if (gwPersonsInitAge >= 0)
      fprintf(pOutStream, "%d - ", gwPersonsInitAge);
   else
      fprintf(pOutStream, "- - ");

   if (gwPersonsAgeAtDeath >= 0)
      fprintf(pOutStream, "%d\n", gwPersonsAgeAtDeath);
   else
      fprintf(pOutStream, "-\n");

   fprintf(pOutStream, "Age  !");

   for (i = 0; i < 17; i++)
      fprintf(pOutStream, "----+");
   wStopAge = wSIM_CUTOFF_YEAR - gwPersonsYOB;

   if (gwPersonsAgeAtDeath != 0)
      fprintf(pOutStream, "\n%4d !", gwPersonsYOB);
   else
      fprintf(pOutStream, "\n%4d X", gwPersonsYOB);

   if ( gwPersonsInitAge >= 0) {
      for (i = 1; i < gwPersonsInitAge; i++) {
         if (i != gwPersonsAgeAtDeath)
            fprintf(pOutStream,"-");
         else
            fprintf(pOutStream,"X");
      }
      if (gwPersonsCessAge >= 0) {
         for (i = gwPersonsInitAge; i < gwPersonsCessAge; i++) {
            if (i != gwPersonsAgeAtDeath)
               fprintf(pOutStream,"s");
            else
               fprintf(pOutStream,"X");
         }
         for (i = gwPersonsCessAge; i <= wStopAge; i++) {
            if (i != gwPersonsAgeAtDeath)
               fprintf(pOutStream, "q");
            else
               fprintf(pOutStream, "X");
         }
      } else {
         for (i = gwPersonsInitAge; i <= wStopAge; i++) {
            if (i != gwPersonsAgeAtDeath)
               fprintf(pOutStream, "s");
            else
               fprintf(pOutStream, "X");
            }
      }
   } else {
      for (i = 1; i <= wStopAge; i++) {
         if (i != gwPersonsAgeAtDeath)
            fprintf(pOutStream, "-");
         else
            fprintf(pOutStream, "X");
         }
      }
   fprintf(pOutStream,"!%d\n", wSIM_CUTOFF_YEAR);
   fprintf(pOutStream,"!The average cigarettes smoked per day by age is not available with this type of output\n");
}


// Write the results to pOutStream in a XML style tagged format
void  Smoking_Simulator::WriteAsXML(FILE *pOutStream) {
   short wYearsAsSmoker, i;
   if (pOutStream == 0) {
      throw SimException("WriteAsTimeline(FILE *)", "Supplied output File is not open for writing.");
   }
   fprintf(pOutStream, "<RESULT>\n");
   fprintf(pOutStream, "<INITIATION_AGE>\n%d\n</INITIATION_AGE>\n", gwPersonsInitAge);
   fprintf(pOutStream, "<CESSATION_AGE>\n%d\n</CESSATION_AGE>\n", gwPersonsCessAge);
   fprintf(pOutStream, "<OCD_AGE>\n%d\n</OCD_AGE>\n", gwPersonsAgeAtDeath);
   if (gwPersonsInitAge >= 0) {
      fprintf(pOutStream, "<SMOKING_HIST>\n");
      fprintf(pOutStream, "<INTENSITY>\n");
      fprintf(pOutStream, "Not applicable in SHG v2\n"); 
      fprintf(pOutStream, "</INTENSITY>\n");

      if (gwPersonsCessAge == -999) // Person does not quit smoking
         wYearsAsSmoker = (wSIM_CUTOFF_YEAR - (gwPersonsYOB+gwPersonsInitAge))+1;
      else
         wYearsAsSmoker = (gwPersonsCessAge - gwPersonsInitAge) + 1;

      // Print out number of age_CPD Combos to expect
      fprintf(pOutStream, "<AGE_CPD_COUNT>\n%d\n</AGE_CPD_COUNT>\n", wYearsAsSmoker);
      for (i = 0; i < wYearsAsSmoker; i++) {
         if (i + gwPersonsInitAge < 100) {
            fprintf(pOutStream, "<AGE_CPD>\n");
            fprintf(pOutStream, "<AGE>\n%d\n</AGE>\n", (i+gwPersonsInitAge));
            fprintf(pOutStream, "<CPD>\n%f\n</CPD>\n", gdPersonsCPDbyAge[i]);
            fprintf(pOutStream, "</AGE_CPD>\n");
         }
      }
      fprintf(pOutStream, "</SMOKING_HIST>\n");
   }
   fprintf(pOutStream, "</RESULT>\n");
}

// Write the results to pOutStream in a data style format
void Smoking_Simulator::WriteAsData(FILE *pOutStream) {
   short wYearsAsSmoker, i;
   if (pOutStream == 0) {
      throw SimException("WriteAsData(FILE *)", "Supplied output File is not open for writing.");
   }

   fprintf(pOutStream, "%d;%d;%d;%d;%d;%d;", gwPersonsRace, gwPersonsSex, gwPersonsYOB, \
                       gwPersonsInitAge, gwPersonsCessAge, gwPersonsAgeAtDeath);

   // Print out the smoking intensity group for the person and the cigarettes smoked per day
   // Print the intensity group as +1 its value so range of values is from 1 to 5.
   if (gwPersonsInitAge != -999) {
      if (gwPersonsCessAge == -999) 
         wYearsAsSmoker = wSIM_CUTOFF_YEAR - (gwPersonsYOB + gwPersonsInitAge) + 1;
      else 
         wYearsAsSmoker = gwPersonsCessAge - gwPersonsInitAge + 1;
      for (i = 0; i < wYearsAsSmoker; i++) {
         if (i + gwPersonsInitAge < 100)
            fprintf(pOutStream, "%d;%.2f;", i + gwPersonsInitAge, gdPersonsCPDbyAge[i]);
      }
   }

   fprintf(pOutStream, "\n");
}
