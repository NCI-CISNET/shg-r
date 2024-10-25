// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: main.cpp
// Author: Martin Krapcho & Ben Racine
// E-Mail: KrapchoM@imsweb.com & ben.racine@cornerstonenw.com
// NCI Contact: Rocky Feuer
// Please view the HelpFile.txt file included with this source code for details pertaining to this version

#pragma hdrstop
#pragma argsused

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <limits>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include <string>
#include <sstream>
#include <vector>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <fstream>

#include "smoking_sim.h"
#include "sim_exception.h"
#include "rng_strategy.h"

#define MAX(x) (std::numeric_limits<x>::max())

#define DEFAULT_DATA_DIR "data/10_28_2014/"
#define COUNTERFACTUAL_DATA_DIR "data/counterfactual_inputs_jan_2009/"

// Input file names
#define INITIATION_DATA_FILE "lbc_smokehist_initiation.txt"
#define CESSATION_DATA_FILE "lbc_smokehist_cessation.txt"
#define OTHER_COD_DATA_FILE "lbc_smokehist_oc_mortality.txt"
#define CPD_INTENSITY_PROBS "lbc_smokehist_cpdintensityprobs.txt"  
#define CPD_DATA_FILE "lbc_smokehist_cpd.txt" 

#define VECTOR_DELIMITER ","
#define MAX_NUM_REPS 1000000
#define VERSION_FILE "version.json"

using namespace std;

std::string VERSIONJSON;
std::string VERSION_NUM;
std::string gInputFileName;
bool gWithHoldTags = false;

const short wMIN_IMMEDIATE_CESSATION_YEAR = 1910;  // Minimum Year Value that can be used as the Immediatte Cessation Year
short wSIM_CUTOFF_YEAR = 2200;                     // Cut-off year for the application

const char sSEX_LABELS[2][7]  = {"Male", "Female"};
const char sRACE_LABELS[2][10] = {"All Races", "White"};

// Declaring Function prototypes
char* AssignFilename(const char* sDirectory, const char * sFilename);
short CountVectorValues(char* sDataString);
bool CreateDataFile(const char *sNumToSimulate, const char* sOutFileName, char*);
void Help(const char* sAppName, FILE* pHelpStream);
bool IsPosLongInt(const char *sValue);
bool IsPosShortInt(const char *sValue);
bool IsValidNumReps(const char* sNumReps);
bool IsValidSeed(const char* sSeedValue);
void LoadValue(char* sDest, char* sSource, int iValueNum);
void ModifyCutoffYear(char*);
bool RunFromParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
void RunInfiniteLoop();
void RunInterface();
int RunWebVersion(const char *sInputFileName);
char* Str_toupper(char *s);
char* Str_tolower(char *s);
void Usage();
short min(short, short);
bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*);
bool ValidateParameters(char*, char*, char*, char*, char*, char*, char*, char*, char*, char*);
void WriteInputTag(FILE* , char*, char*, const char*, const char*);
void WriteRunInfoTag(FILE* , std::string, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*, const char*);
void PropagateVersionInformation();

// sim_fprintf_stderr is a function that has the same signature as fprintf and passes along the same arguments to vfprintf
// RCPP does not allow sim_fprintf to be used, so this function is used to replace it
// Include the arg -DIS_RCPP CXXFLAGS in the Makevars file (eg CXXFLAGS = -O3 -DIS_RCPP)
void sim_fprintf_stderr(const char* format, ...) {
   va_list args;
   va_start(args, format);
   #ifdef IS_RCPP
      //Rcpp::Rcout << vfmt::vformat(format, args);
   #else
      vfprintf(stderr, format, args);
   #endif
   va_end(args);
}

// same as above but with no variable replacement args
void sim_simple_stderr(const char* message) {
   #ifdef IS_RCPP
      //Rcpp::Rcout << vfmt::vformat(format, args);
   #else
      fprintf(stderr, message);
   #endif
}

// save as above but for stdout and with no variable replacement args
void sim_simple_stdout(const char* message) {
   #ifdef IS_RCPP
      //Rcpp::Rcout << vfmt::vformat(format, args);
   #else
      fprintf(stdout, message);
   #endif
}

// Removing the main() function for RCPP because it is unwanted
// But we include all the other methods and variables to avoid DRY violations in the Rcpp wrapper
#ifdef IS_RCPP
#else
int main(int argc, char* argv[]) {
	char sErrorMessage[500];
	int iReturnValue;
   FILE* pHelpFile = 0;

   PropagateVersionInformation();

   switch (argc) {

      // No input parameters, run the user-interface version
      case 1:
   		RunInterface();
	   	iReturnValue = 0; break;

      // 1 input parameter, could be a call to
      //  - Run the program for the web-based version of application
      //  - Write the helpfile to a .txt file or to the screen
      //  - Put the program in an infinite loop (used for error testing with the website)
      // Which routine to run is based on the value in argv[1]
      case 2:
         // TODO: Don't allow 
         // char buffer[20];
         // std::string str ("Test string...");
         // std::size_t length = argv[1].copy(buffer,6,5);
         // buffer[length]='\0';
         // std::cout << "buffer contains: " << buffer << '\n';
         /*
         if (strcmp(Str_toupper(argv[1]), "HELP") == 0) {  
            // Write help to screen
            Help(argv[0],stdout);
            iReturnValue = 0;
         } else if (strcmp(Str_toupper(argv[1]), "WRITEHELP") == 0) { // Write help to file
            pHelpFile = fopen("HelpFile.txt", "w");
            if (pHelpFile != NULL) {
               Help(argv[0], pHelpFile);
               fclose(pHelpFile);
               iReturnValue = 0;
            } else {
               iReturnValue = 1;
            }
         } else if (strcmp(Str_toupper(argv[1]), "LOOP")==0) {  
            RunInfiniteLoop();
         } else { 
            argv[1] = Str_tolower(argv[1]);
         */
         iReturnValue = RunWebVersion(argv[1]);
         break;


      // 3 input parameters, Create a data file - FOR TESTING ONLY - NOT TO BE USED IN SIMULATIONS
      case 4:
         if ( (strcmp(argv[1], "CREATE_DATA_FILE") == 0) && CreateDataFile(argv[2], argv[3], sErrorMessage) ) {
      		iReturnValue = 0;
         } else {   // Must have hit an error
      		sim_fprintf_stderr("%s\n", sErrorMessage);
      		iReturnValue = 1;
            getc(stdin);
        	} break;

      case 9:
         // Use Input parameters (no data directory assigned)
         if (ValidateParameters(argv[1], argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage) &&
            RunFromParameters(DEFAULT_DATA_DIR,argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage)) {
            iReturnValue = 0;
         } else {  // We hit an error in Validating or Running the parameters, print error and exit
            sim_fprintf_stderr("%s\n", sErrorMessage);
            iReturnValue = 1;
         } break;

      case 10:
         // Use Input parameters
         if (ValidateParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage) &&
              RunFromParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage)) {
            iReturnValue = 0;
         } else {  // We hit an error in Validating or Running the parameters, print error and exit
            sim_fprintf_stderr("%s\n", sErrorMessage);
            iReturnValue = 1;
         } 
         break;

      case 11:
         ModifyCutoffYear(argv[10]);
         // Use Input parameters (no data directory assigned)
         if (ValidateParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage) &&
             RunFromParameters(DEFAULT_DATA_DIR,argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],sErrorMessage)) {
            iReturnValue = 0;
         } else { // We hit an error in Validating or Running the parameters, print error and exit
            sim_fprintf_stderr("%s\n", sErrorMessage);
            iReturnValue = 1;
         } break;

      case 12:
         ModifyCutoffYear(argv[11]);
         // Use Input parameters
         if (ValidateParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage) &&
              RunFromParameters(argv[1],argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],sErrorMessage)) {
            iReturnValue = 0;
         } else { // We hit an error in Validating or Running the parameters, print error and exit
            sim_fprintf_stderr("%s\n", sErrorMessage);
            iReturnValue = 1;
         } 
         break;

      default:
   		Usage();
         break;
      }

   return iReturnValue;
}
#endif

// Returns a string containing the directory and filename concatenated together
char* AssignFilename(const char* sDirectory, const char * sFilename) {
   int iCurrIndex, i;
   char* sFullFilePath;
   sFullFilePath = new char[strlen(sDirectory) + strlen(sFilename) + 2];
   iCurrIndex = 0;
   for (i=0; i <(strlen(sDirectory)); i++) {
      sFullFilePath[iCurrIndex] = sDirectory[i];
      iCurrIndex++;
   }
   #ifdef WIN32  // windows code goes here
      if (sFullFilePath[iCurrIndex-1] != '\\') {
         sFullFilePath[iCurrIndex] = '\\';
         iCurrIndex++;
      }
   #else // unix code goes here
      if (sFullFilePath[iCurrIndex-1] != '/') {
         sFullFilePath[iCurrIndex] = '/';
         iCurrIndex++;
      }
   #endif
   for (i=0; i <(strlen(sFilename)); i++) {
      sFullFilePath[iCurrIndex] = sFilename[i];
      iCurrIndex++;
   }
   sFullFilePath[iCurrIndex] = '\0';
   return sFullFilePath;
}

// Returns the number of data values contained in sDataString
short CountVectorValues(char* sDataString) {
   short wReturnValue = 0;
   char  *pTokenPtr = 0,
         *sBuffer   = 0;
          size_t i;

   if (sDataString != NULL) {  // Designed so that the ERStatus string will count as 1 if missing
      sBuffer = new char[strlen(sDataString)+1];
      strcpy(sBuffer, sDataString);
      pTokenPtr = strtok(sBuffer, VECTOR_DELIMITER);
      while (pTokenPtr != NULL) {
         wReturnValue++;
         pTokenPtr = strtok(NULL, VECTOR_DELIMITER);
      }
   }
   delete [] sBuffer;
   return wReturnValue;
}


// Write the help text to FILE*
void Help(const char* sAppName,FILE* pOutStream) {
   sim_fprintf(pOutStream, "\nCancer Intervention and Surveillance Modeling Network\n");
   sim_fprintf(pOutStream, "(CISNET)\n");
   sim_fprintf(pOutStream, "Lung Cancer Base Case\n\n");
   sim_fprintf(pOutStream, "Smoking History Generator Application\n");
   sim_fprintf(pOutStream, "The use of immediate cessation has changed with this release.\n");
   sim_fprintf(pOutStream, "To apply immediatte cessation, the year for immediate cessation must now be supplied to the application.\n");
   sim_fprintf(pOutStream, "The year value is now supplied as the last input parameter (See Section 2 below).\n");
   sim_fprintf(pOutStream, "If the year value supplied is '0', immediate cessation will not be used in the run.\n");
   sim_fprintf(pOutStream, "If a year value is supplied, Immediatte Cessation will occur on January 1st of year provided.\n\n");
   sim_fprintf(pOutStream, "Section 1: Usage\n\n");
   sim_fprintf(pOutStream, "1. User Interface Mode\n");
   sim_fprintf(pOutStream, "Type:  %s\n\n",sAppName);
   sim_fprintf(pOutStream, "2. Command Line Mode\n");
   sim_fprintf(pOutStream, "Type: %s Source_Dir Init_Seed Cess_Seed Oth_Cod_Seed Indiv_Seed Input_File Output_File Output_Type Immediate_Cessation\n",sAppName);
   sim_fprintf(pOutStream, " or\n");
   sim_fprintf(pOutStream, "Type: %s Init_Seed Cess_Seed Oth_Cod_Seed Indiv_Seed Input_File Output_File Output_Type Immediate_Cessation\n",sAppName);
   sim_fprintf(pOutStream, "Where:\n");
   sim_fprintf(pOutStream, "\tSource_Dir     - Directory containing the NHIS or counterfactual inputs for the simulation model. Application will use the NHIS estiamtes if this value is ommitted.\n");
   sim_fprintf(pOutStream, "\tInit_Seed      - An integer seed for the Initiation Probability PRNG (>= 0)\n");
   sim_fprintf(pOutStream, "\tCess_Seed      - An integer seed for the Cessation Probability PRNG (>= 0)\n");
   sim_fprintf(pOutStream, "\tOth_Cod_Seed   - An integer seed for the Other Cause of Death Probability PRNG (>=0)\n");
   sim_fprintf(pOutStream, "\tIndiv_Seed     - An integer seed for the PRNG that will be used for defining characteristics of the individual (>= 0).\n");
   sim_fprintf(pOutStream, "\tInput_File     - Name of file containing the covariate combinations to simulate. Should be formatted using Input File Format 1 (defined below).\n");
   sim_fprintf(pOutStream, "\tOutput_File    - Name of the output file that the application should write to.\n");
   sim_fprintf(pOutStream, "\tOutput_Type    - Style of output to write: 1 = Data ,  2 = Text,  3 = Timeline\n");
   sim_fprintf(pOutStream, "\tCessation_Year - 4-digit Year Value. All smokers will stop smoking on January 1st of year provided. Enter a value of '0' to disable the immediate cessation option.\n\n");
   sim_fprintf(pOutStream, "3. Web Interface Mode\n");
   sim_fprintf(pOutStream, "NOTE: This mode was designed for use with a website. It will provide the same results but it does have\n");
   sim_fprintf(pOutStream, "\tdifferent requirements in terms of how the input to the program should be formatted and the results\n");
   sim_fprintf(pOutStream, "\tare presented within HTML style tags.\n");
   sim_fprintf(pOutStream, "Type: %s INFILE_PATH\n",sAppName);
   sim_fprintf(pOutStream, "Where:\n");
   sim_fprintf(pOutStream, "\tINFILE_PATH = Path to the input file to be used for the application\n");
   sim_fprintf(pOutStream, "\tThis input file must be formatted using Input File Format 2 (defined below).\n\n");
   sim_fprintf(pOutStream, "4. Additional calls\n");
   sim_fprintf(pOutStream, "Type: %s Loop\n",sAppName);
   sim_fprintf(pOutStream, "\t- Force the application into an infinite loop\n");
   sim_fprintf(pOutStream, "Type: %s Help\n",sAppName);
   sim_fprintf(pOutStream, "\t- Calls this help writing function.\n\n");
   sim_fprintf(pOutStream, "The application returns a value of 0 upon successful completion\n");
   sim_fprintf(pOutStream, " and a value of 1 if an error occurred.\n");
   sim_fprintf(pOutStream, "\n\n");
   sim_fprintf(pOutStream, "Section 2: Input File Formats\n\n");
   sim_fprintf(pOutStream, "Input File Format 1:\n");
   sim_fprintf(pOutStream, "This format is required for Usage: \n");
   sim_fprintf(pOutStream, "\t%s Source_Dir Init_Seed Cess_Seed Oth_Cod_Seed Indiv_Seed Input_File Output_File Output_Type\n",sAppName);
   sim_fprintf(pOutStream, "The input file needs to a DOS formatted text file.\n");
   sim_fprintf(pOutStream, "Only one record per line is allowed.\n");
   sim_fprintf(pOutStream, "Values in a record must be semi-colon delimited integer values.\n");
   sim_fprintf(pOutStream, "Record Layout:\n");
   sim_fprintf(pOutStream, "\tRace, Sex, Year Of Birth\n");
   sim_fprintf(pOutStream, "Acceptable Values for Record Variables:\n");
   sim_fprintf(pOutStream, "Variable		  Values       Formats\n");
   sim_fprintf(pOutStream, "Race           0,           (All Races)\n");
   sim_fprintf(pOutStream, "Sex            0, 1         (Male, Female)\n");
   sim_fprintf(pOutStream, "Year of Birth  1890-1984\n");
   sim_fprintf(pOutStream, "Record Example:\n");
   sim_fprintf(pOutStream, "0,1,1956\n");
   sim_fprintf(pOutStream, "(Female born in 1956)\n\n");
   sim_fprintf(pOutStream, "Input File Format 2 (for the web-based interface):\n");
   sim_fprintf(pOutStream, "This format is required for Usage: \n");
   sim_fprintf(pOutStream, "\t%s INFILE_PATH\n\n",sAppName);
   sim_fprintf(pOutStream, "KEY VALUE\n\n");
   sim_fprintf(pOutStream, "Keys are not case-sensitive.\n");
   sim_fprintf(pOutStream, "Valid keys for Input File:\n");
   sim_fprintf(pOutStream, "Key               Description\n");
   sim_fprintf(pOutStream, "--------------------------------------------------------\n");
   sim_fprintf(pOutStream, "SEED_INIT=     Seed value for PRNG used for Initiation Probabilitie\n");
   sim_fprintf(pOutStream, "SEED_CESS=     Seed for PRNG used for Cessation Probabilities\n");
   sim_fprintf(pOutStream, "SEED_OCD=      Seed for PRNG used for Other COD Probabilities\n");
   sim_fprintf(pOutStream, "SEED_MISC=     Seed for PRNG used to generate misc. random variables needed by app.\n");
   sim_fprintf(pOutStream, "RACE=          Race (Valid Values listed below)\n");
   sim_fprintf(pOutStream, "SEX=           Sex  (Valid Values listed below)\n");
   sim_fprintf(pOutStream, "YOB=           Year of Birth (Valid Values listed below)\n");
   sim_fprintf(pOutStream, "CESSATION_YR=  Year value that forces smokers to quit on January 1st of that year. Enter '0' to disable immediate cessation\n");
   sim_fprintf(pOutStream, "REPEAT=        Number of times to repeat simulation parameters (Optional)\n");
   sim_fprintf(pOutStream, "INIT_PROB=     File containing the initiation probabilities\n");
   sim_fprintf(pOutStream, "CESS_PROB=     File containing the cessation probabilities\n");
   sim_fprintf(pOutStream, "OCD_PROB=      File containing the other COD probabilities\n");
   sim_fprintf(pOutStream, "CPD_QUINTILES= File containing the smoking quintile probabilities\n");
   sim_fprintf(pOutStream, "CPD_DATA=      File containing cigarette per day values\n");
   sim_fprintf(pOutStream, "OUTPUTFILE=    Output file name\n");
   sim_fprintf(pOutStream, "ERRORFILE=     Error log\n\n");
   sim_fprintf(pOutStream, "RNGSTRATEGY=   Random Number Generator Strategy (MersenneTwister or RngStream)\n\n");
   sim_fprintf(pOutStream, "The repeat= key is optional and can be excluded.\n");
   sim_fprintf(pOutStream, "\n\n");
   sim_fprintf(pOutStream, "Section 3: Valid Values for Select Keys\n\n");
   sim_fprintf(pOutStream, "Key            Valid Values\n");
   sim_fprintf(pOutStream, "--------------------------------------------------------\n");
   sim_fprintf(pOutStream, "SEED_INIT=     Integer from -1 to %ld\n", MAX(long));
   sim_fprintf(pOutStream, "               A value of -1 uses the clock time as the seed\n");
   sim_fprintf(pOutStream, "SEED_CESS=     Same as SEED_INIT\n");
   sim_fprintf(pOutStream, "SEED_OCD=      Same as SEED_INIT\n");
   sim_fprintf(pOutStream, "SEED_MISC=     Same as SEED_INIT\n\n");
   sim_fprintf(pOutStream, "RACE=          0\n");
   sim_fprintf(pOutStream, "               (0 = All Races)\n\n");
   sim_fprintf(pOutStream, "SEX=           0, 1\n");
   sim_fprintf(pOutStream, "               (0 = Male)\n");
   sim_fprintf(pOutStream, "               (1 = Female)\n\n");
   sim_fprintf(pOutStream, "YOB=           Integer from 1890 to 1984\n\n");
   sim_fprintf(pOutStream, "CESSATION_YR=  Integer from %d to %d\n\n",wMIN_IMMEDIATE_CESSATION_YEAR,wSIM_CUTOFF_YEAR);
   sim_fprintf(pOutStream, "\n\n");
   sim_fprintf(pOutStream, "Section 4: Using Vector Values\n\n");
   sim_fprintf(pOutStream, "The following keys can contain multiple inputs in a comma-delimited vector:\n");
   sim_fprintf(pOutStream, "  RACE\n");
   sim_fprintf(pOutStream, "  SEX\n");
   sim_fprintf(pOutStream, "  YOB\n");
   sim_fprintf(pOutStream, "  REPEAT\n\n");
   sim_fprintf(pOutStream, "Vector Notes/Restrictions:\n\n");
   sim_fprintf(pOutStream, "  Vectors may be used for more than 1 key, but the number of values\n");
   sim_fprintf(pOutStream, "    in each key must be equivalent.\n");
   sim_fprintf(pOutStream, "  The keys that do not use vectors must still have one value\n");
   sim_fprintf(pOutStream, "    REPEAT is still optional as explained in Section 2.\n");
   sim_fprintf(pOutStream, "  If the REPEAT value is included and is not a vector value, each set of\n");
   sim_fprintf(pOutStream, "    parameters will be repeated by the amount specified.\n");
   sim_fprintf(pOutStream, "  If the REPEAT value is included and is a vector value, the repeat\n");
   sim_fprintf(pOutStream, "    value will pertain to the value set that it corresponds to.\n\n\n");
   sim_fprintf(pOutStream, "\n\n");
   sim_fprintf(pOutStream, "Section 5: Output File Tags\n\n");
   sim_fprintf(pOutStream, "  In the output file, the information is written within XML-style tags\n");
   sim_fprintf(pOutStream, "  This section will outline the valid tags and the content written inside of these tags.\n\n");
   sim_fprintf(pOutStream, "  Tag                 Parent Tag     Content\n");
   sim_fprintf(pOutStream, "----------------------------------------------------------------------------------------\n");
   sim_fprintf(pOutStream, "  <RUNINFO>           N/A            Run info for the software including version, seeds and datafiles.\n");
   sim_fprintf(pOutStream, "  <VERSION>           <RUNINFO>      Software version number.\n");
   sim_fprintf(pOutStream, "  <SEEDS>             <RUNINFO>      Seeds used for this run of the application.\n");
   sim_fprintf(pOutStream, "  <INIT_PRNG_SEED>    <SEEDS>        Seed used for Initiation PRNG.\n");
   sim_fprintf(pOutStream, "  <CESS_PRNG_SEED>    <SEEDS>        Seed used for Cessation PRNG.\n");
   sim_fprintf(pOutStream, "  <OCD_PRNG_SEED>     <SEEDS>        Seed used for Other Cause of Death PRNG.\n");
   sim_fprintf(pOutStream, "  <MISC_PRNG_SEED>    <SEEDS>        Seed used for Other PRNs used by the application.\n");
   sim_fprintf(pOutStream, "  <DATAFILES>         <RUNINFO>      Datafiles used by this run of the application.\n");
   sim_fprintf(pOutStream, "  <INITIATION>        <DATAFILES>    Initiation Probablities File.\n");
   sim_fprintf(pOutStream, "  <CESSATION>         <DATAFILES>    Cessation Probablities File.\n");
   sim_fprintf(pOutStream, "  <OCD>               <DATAFILES>    Other Cause of Death Probabilities File.\n");
   sim_fprintf(pOutStream, "  <QUINTILES>         <DATAFILES>    Smoking Intensity Quintile Probabilities File.\n");
   sim_fprintf(pOutStream, "  <CIG_PER_DAY>       <DATAFILES>    Cigarettes per Day Datafile.\n");
   sim_fprintf(pOutStream, "  <OPTIONS>           <RUNINFO>      Run Options. Affects all runs done by program.\n");
   sim_fprintf(pOutStream, "  <CESSATION_YR>      <OPTIONS>      Immediate Cessation Year. 0 = Immediate cessation not used.\n");
   sim_fprintf(pOutStream, "  <OUTFILES>          <RUNINFO>      Files created by the application.\n");
   sim_fprintf(pOutStream, "  <OUTPUT>            <OUTFILES>     Output File.\n");
   sim_fprintf(pOutStream, "  <ERRORS>            <OUTFILES>     Error Log.\n");
   sim_fprintf(pOutStream, "  <SIMULATION>        N/A            Encapsulates a simulation run for a set of inputs.\n");
   sim_fprintf(pOutStream, "  <INPUTS>            <SIMULATION>   Inputs for the simulation block.\n");
   sim_fprintf(pOutStream, "  <RACE>              <INPUTS>       Race\n");
   sim_fprintf(pOutStream, "  <SEX>               <INPUTS>       Sex\n");
   sim_fprintf(pOutStream, "  <YOB>               <INPUTS>       YOB\n");
   sim_fprintf(pOutStream, "  <REPEAT>            <INPUTS>       Number of times the simulation is run for given inputs.\n");
   sim_fprintf(pOutStream, "  <RUNS>              <SIMULATION>   Encapsulates the results for the simulation block.\n");
   sim_fprintf(pOutStream, "  <RESULT>            <RUNS>         Encapsulates the results for a simulated individual.\n");
   sim_fprintf(pOutStream, "  <INITIATION_AGE>    <RESULT>       Age at smoking initiation (-999 = N/A).\n");
   sim_fprintf(pOutStream, "  <CESSATION_AGE>     <RESULT>       Age at smoking cessation (-999 = N/A).\n");
   sim_fprintf(pOutStream, "  <OCD_AGE>           <RESULT>       Age at death from cause other than lung cancer (-999 = Still Alive).\n");
   sim_fprintf(pOutStream, "  <SMOKING_HIST>      <RESULT>       Encapsulates the smoking history for the individual.\n");
   sim_fprintf(pOutStream, "  <INTENSITY>         <SMOKING_HIST> Smoking Intesity. 5 groups ranging from light to heavy smoker.\n");
   sim_fprintf(pOutStream, "  <AGE_CPD_COUNT>     <SMOKING_HIST> Number of age/cigarette per day combos in smoking history.\n");
   sim_fprintf(pOutStream, "  <AGE_CPD>           <SMOKING_HIST> Encapsulates an age/cigarette per day, combination.\n");
   sim_fprintf(pOutStream, "  <AGE>               <AGE_CPD>      Age value for age-cigaretters per day combination.\n");
   sim_fprintf(pOutStream, "  <AGE>               <AGE_CPD>      Cigaretters smoked per day for age in corresponding <AGE> tag.\n");
   sim_fprintf(pOutStream, "\n\n");
   sim_fprintf(pOutStream, "Section 6: Version History\n\n");
   sim_fprintf(pOutStream, "Version 6.0.0 (May 2012) - \n");
   sim_fprintf(pOutStream, "Code was modified to be compatible with Linux compiler GCC version 3.4.4. Includes modifications to include files and \n");
   sim_fprintf(pOutStream, "implementation of a string to upper and lower case functions that were not available in standard headers for Linux compiler.\n");
   sim_fprintf(pOutStream, "Version 5.2.1 (January 2009) - \n");
   sim_fprintf(pOutStream, "Fixed a bug in the ValidateParameters function in main.cpp. Function did not accept '0' as a valid immediate cessation value.\n");
   sim_fprintf(pOutStream, "Version 5.2.0 (January 2009) - \n");
   sim_fprintf(pOutStream, "Immediate cessation was changed to allow the user to specify the year of immediate cessation. \n");
   sim_fprintf(pOutStream, "NHIS and Counterfactual estimates were modified to include year of birth cohorts 1890-1894 and 1895-1899. \n");
   sim_fprintf(pOutStream, "Application is now limited to producing simulations for All Races Males and All Races Females. \n");
   sim_fprintf(pOutStream, "Version 5.1.0 (September 2008) - \n");
   sim_fprintf(pOutStream, "Counterfactual estimates for All Race Male and All Races Female were added to the application. \n");
   sim_fprintf(pOutStream, "Version 5.0.0 (July 2008) - \n");
   sim_fprintf(pOutStream, "Smoking History Application modified to include an immediate cessation option. \n");
   sim_fprintf(pOutStream, "NHIS Inputs for All Races Male and All Races Female were added to the project. \n");
   sim_fprintf(pOutStream, "Version 4.0.0 (February 2008) - \n");
   sim_fprintf(pOutStream, "Smoking History Application modified for use with the counterfactual inputs. \n");
   sim_fprintf(pOutStream, "\tUsers can specify the source directory for this applications input data files.\n");
   sim_fprintf(pOutStream, "\tCounterfactual inputs were formatted for use with this application and supplied with the application.\n\n");
   sim_fprintf(pOutStream, "Version 3.2.0 (May 2006) - \n");
   sim_fprintf(pOutStream, "Smoking History Application modified for use with the CISNET Parameter \n");
   sim_fprintf(pOutStream, "Generator Model Interface website\n");
   sim_fprintf(pOutStream, "\tProgram was modifed to read from an input file provided by the website.\n");
   sim_fprintf(pOutStream, "\tProgram was modifed write output in an XML style format.\n");
}

void LoadValue(char* sDest, char* sSource, int iValueNum) {
   bool bReturnValue;
   char *pTokenPtr = 0,
        *sBuffer = 0;
   int i;

   sBuffer = new char[strlen(sSource) + 1];
   strcpy(sBuffer, sSource);
   pTokenPtr = strtok(sBuffer, ",");
   if (iValueNum == 0) {
      strcpy(sDest, pTokenPtr);
   } else {
      for (i = 1; i <= iValueNum; i++) {
	      pTokenPtr = strtok(NULL, ",");
	      if (i == iValueNum) {
            strcpy(sDest, pTokenPtr);
         }
	   }
   }
   delete [] sBuffer;
}

// Run the application using the seeds and input/output stream
bool RunFromParameters(char* sDataFileDir, char* sInitiationSeed,
                      char* sCessationSeed, char* sOtherCODSeed,
                      char* sIndivRndSeed, char* sInputFile,
                      char* sOutputFile, char* sOutputType,
                      char* sImmediateCess, char* sErrorMessage) {

	bool						bReturnValue = true;
   short                wOutputType,
                        wCessationYear;
	unsigned long 			ulInitiationSeed,
					  			ulCessationSeed,
                        ulOtherCODSeed,
                        ulIndivRndSeed;
   char                *sInitiationFile = 0,
                       *sCessationFile = 0,
                       *sOtherCODFile = 0,
                       *sCPDIntensityFile = 0,
                       *sCPDDataFile = 0;
	Smoking_Simulator	  *pSimulator  = 0;

	try {
      sInitiationFile = AssignFilename(sDataFileDir, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(sDataFileDir, CESSATION_DATA_FILE);
      sOtherCODFile = AssignFilename(sDataFileDir, OTHER_COD_DATA_FILE);
      sCPDIntensityFile = AssignFilename(sDataFileDir, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(sDataFileDir, CPD_DATA_FILE);
      ulInitiationSeed = (unsigned long) atol(sInitiationSeed);
      ulCessationSeed = (unsigned long) atol(sCessationSeed);
      ulOtherCODSeed = (unsigned long) atol(sOtherCODSeed);
      ulIndivRndSeed = (unsigned long) atol(sIndivRndSeed);
      wOutputType = (short) atoi(sOutputType);
      wCessationYear = (short) atoi(sImmediateCess);

  		pSimulator = new Smoking_Simulator(sInitiationFile, sCessationFile, sOtherCODFile, sCPDIntensityFile, sCPDDataFile, 
                                         ulInitiationSeed, ulCessationSeed, ulOtherCODSeed, ulIndivRndSeed,  
                                         wOutputType, wCessationYear);


      pSimulator->RunSimulation(sInputFile, sOutputFile, false);

   } catch (SimException ex) {
      sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "%s", ex.GetError());
		bReturnValue = false;
   } catch(...) {
      sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Unknown Error Occurred\n");
		bReturnValue = false;
   }

	delete pSimulator;
   delete [] sInitiationFile; delete [] sCessationFile; delete [] sOtherCODFile; delete [] sCPDIntensityFile; delete [] sCPDDataFile;
	return bReturnValue;
}

// Verify that a string value is a valid positive long integer
bool IsPosLongInt(const char* sValue) {
   char sUpperValue[100];  // Max long value is shorter than 100 digits
   bool bReturnValue;
   long lUpperValue = MAX(long);

   sim_snprintf(sUpperValue, sizeof(sUpperValue), "%ld", lUpperValue);

   bReturnValue = ((strspn( sValue, "0123456789" ) == strlen(sValue)) &&
                   ((strlen(sValue) < strlen(sUpperValue)) ||
                    ((strlen(sValue) == strlen(sUpperValue)) &&
                     (strcmp(sValue,sUpperValue)<=0))));

   return bReturnValue;
}

// Verify that a string value is a valid positive short integer
bool IsPosShortInt(const char* sValue) {
   char 	sUpperValue[100]; // Max short int value is shorter than 100 digits
   bool 	bReturnValue;
   short wUpperValue      = MAX(short);
   sim_snprintf(sUpperValue, sizeof(sUpperValue), "%hd", wUpperValue);
   bReturnValue = ((strspn( sValue, "0123456789" ) == strlen(sValue)) &&
                   ((strlen(sValue) < strlen(sUpperValue)) ||
                    ((strlen(sValue) == strlen(sUpperValue)) &&
                     (strcmp(sValue,sUpperValue) <= 0))));
   return bReturnValue;
}

// Verify the repeat= value is a valid input
bool IsValidNumReps(const char* sNumReps) {
   long wNumReps;
   bool bReturnValue= false;
   if (IsPosLongInt(sNumReps)) {
      wNumReps = atoi(sNumReps);
      if (wNumReps <= MAX_NUM_REPS)
         bReturnValue = true;
   }
   return bReturnValue;
}

// Verify that the seed= value is a valid input
bool IsValidSeed(const char* sSeedValue) {
   bool bReturnValue;
   bReturnValue = (strcmp(sSeedValue,"-1") == 0) || (IsPosLongInt(sSeedValue));
   return bReturnValue;
}

// Testing function - Runs an infinite loop
// Provided so that the calling function can tests its actions when this app does not respond after a set time
void RunInfiniteLoop() {
   bool bCanStop = false;
   while (!bCanStop);
}

//---------------------------------------------------------------------------
void RunInterface() {
   Smoking_Simulator* 	pSimulator = 0;
   char           		sInputChar[101],
                  		sTempFileName[101],
                  		sOutputFileName[105],
                  		sInputFileName[105],
                  		sExtensionCheck[5],
                       *sInitiationFile = 0,
                       *sCessationFile = 0,
                       *sOtherCODFile = 0,
                       *sCPDIntensityFile = 0,
                       *sCPDDataFile = 0;
   bool                 bValidInput,
                  		bKeepRepeating;
   unsigned long  		ulInitPRNGSeed,
                  		ulCessPRNGSeed,
                        ulOthCODSeed,
                        ulIndivRndSeed;
   double         		dTempValue;
   short                wSourceData,
                        wTempValue,
                  		wInputSex,
                  		wInputRace,
                  		wInputYOB,
                  		wInputOutputType,
                        wOutputFormat,
                        wNumCigsPerDay,
                        wCessationYear,
                  		i;
   long           		lExtCheckPosition,
                  		lNumRepetitions;
   FILE*          		pOutputFile = 0;

   sim_fprintf(stdout,"Smoking History Simulator\n\n");

   bValidInput = false;
   wCessationYear = 0; // 0 = do not use immediate cessation.
   sim_fprintf(stdout,"\nSelect which estimates to use as the model inputs:\n");
   sim_fprintf(stdout,"1 - NHIS estimates.\n");
   sim_fprintf(stdout,"2 - Counterfactual estimates.\n");
   sim_fprintf(stdout,"3 - Immediate Cessation using NHIS estimates.\n");
   sim_fprintf(stdout,"(Please enter 1, 2 or 3):\n");
   while (!bValidInput) {
      fgets(sInputChar, 10, stdin);
      if ( IsPosShortInt(sInputChar) && ((atoi(sInputChar) >= 1) && (atoi(sInputChar) <= 3))) {
         wSourceData = atoi(sInputChar);
         bValidInput = true;
         if (wSourceData == 3) {
            sim_fprintf(stdout,"\nEnter a year to use for immediate cessation.\nAll smokers will quit smoking on Jan 1st of this year.\n(Please enter a year in the range %d-%d):\n",
                    wMIN_IMMEDIATE_CESSATION_YEAR,wSIM_CUTOFF_YEAR);
            bValidInput = false;
            while (!bValidInput) {
               fgets(sInputChar, 10, stdin);
               if ( IsPosShortInt(sInputChar) &&
                    ((atoi(sInputChar) >= wMIN_IMMEDIATE_CESSATION_YEAR) &&
                     (atoi(sInputChar) <= wSIM_CUTOFF_YEAR)))
                  {
                  wCessationYear = atoi(sInputChar);
                  bValidInput = true;
                  }
               else
                  sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value between %d and %d:\n",sInputChar,wMIN_IMMEDIATE_CESSATION_YEAR,wSIM_CUTOFF_YEAR);
               }

            }
         }
      else
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter either 1, 2 or 3:\n", sInputChar);
      }

   /* Load the filenames for the application */
   if (wSourceData == 2) {
      sInitiationFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CESSATION_DATA_FILE);
      sOtherCODFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, OTHER_COD_DATA_FILE);
      sCPDIntensityFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(COUNTERFACTUAL_DATA_DIR, CPD_DATA_FILE);
   } else {
      sInitiationFile = AssignFilename(DEFAULT_DATA_DIR, INITIATION_DATA_FILE);
      sCessationFile = AssignFilename(DEFAULT_DATA_DIR, CESSATION_DATA_FILE);
      sOtherCODFile = AssignFilename(DEFAULT_DATA_DIR, OTHER_COD_DATA_FILE);
      sCPDIntensityFile = AssignFilename(DEFAULT_DATA_DIR, CPD_INTENSITY_PROBS);
      sCPDDataFile = AssignFilename(DEFAULT_DATA_DIR, CPD_DATA_FILE);
   }

   bValidInput         = false;
   sim_fprintf(stdout,"\nRandom Number Generator Seeds:\n");
   sim_fprintf(stdout,"Please enter a seed for the PRNG that generates Initiation Probabilities.\n");
   sim_fprintf(stdout,"Seed should be in range 0 - %ld.\n:",MAX(long));
   while (!bValidInput)
      {
      fgets(sInputChar, 100, stdin);
      if ( IsPosLongInt(sInputChar)) {
         ulInitPRNGSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
         }
      else
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   sim_fprintf(stdout,"Please enter a seed for the PRNG that generates Cessation Probabilities.\n");
   sim_fprintf(stdout,"Seed should be in range 0 - %ld.\n:",MAX(long));
   while (!bValidInput) {
      fgets(sInputChar, 100, stdin);
      if (IsPosLongInt(sInputChar)) {
         ulCessPRNGSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
      }
      else
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   sim_fprintf(stdout,"Please enter a seed for the PRNG that generates \nnon-lung cancer death probabilities.\n");
   sim_fprintf(stdout,"Seed should be in range 0 - %ld.\n:",MAX(long));
   while (!bValidInput) {
      fgets(sInputChar, 100, stdin);
      if (IsPosLongInt(sInputChar))
         {
         ulOthCODSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
         }
      else
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   sim_fprintf(stdout,"Please enter a seed for the PRNG that generates \nunique random numbers for the simulated individual.\n");
   sim_fprintf(stdout,"This PRNG is for defining characteristics such as \nwill the person be a light or heavy smoker.\n");
   sim_fprintf(stdout,"Seed should be in range 0 - %ld.\n:",MAX(long));
   while (!bValidInput) {
      fgets(sInputChar, 100, stdin);
      if ( IsPosLongInt(sInputChar))
         {
         ulIndivRndSeed = (unsigned long) atol(sInputChar);
         bValidInput = true;
         }
      else
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value in range 0 - %ld.\n:",
                 sInputChar,MAX(long));
      }

   bValidInput = false;
   sim_fprintf(stdout,"\nData Input and Output Options:\n");
   sim_fprintf(stdout,"1 - Read values from a file and write results to an output file.\n");
   sim_fprintf(stdout,"2 - Read values from a file and write results to the screen only.\n");
   sim_fprintf(stdout,"3 - Manually enter Sex, Race and Year of Birth Values \n");
   sim_fprintf(stdout,"    and write results to an output file.\n");
   sim_fprintf(stdout,"4 - Manually enter Sex, Race and Year of Birth Values\n");
   sim_fprintf(stdout,"    and write results to the screen only.\n");
   sim_fprintf(stdout,"(Please enter 1 to 4):\n");

   while (!bValidInput) {fgets(sInputChar, 10, stdin);
      if ( IsPosShortInt(sInputChar) && ((atoi(sInputChar) >= 1) && (atoi(sInputChar) <= 4))) {
         wInputOutputType = atoi(sInputChar);
         bValidInput = true;
      } else {
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value 1 through 4:\n", sInputChar);
      }
   }

   if (wInputOutputType == 1 || wInputOutputType == 2) {
      sim_fprintf(stdout,"\nSpecify input filename (100 char max):\n");
      fgets(sInputChar, 100, stdin);
      strcpy(sInputFileName,sInputChar);
   }

   if (wInputOutputType == 1 || wInputOutputType == 3) {
      sim_fprintf(stdout,"Specify an output filename (100 char max):\n");
      fgets(sInputChar, 100, stdin);

      // Verify a .txt extension, if not, add one.
      if (strlen(sInputChar) > 4) {
         lExtCheckPosition = strlen(sInputChar) - 4;
         for (i=0; i <=3; i++)
            {
            sExtensionCheck[i] = toupper(sInputChar[lExtCheckPosition + i]);
            }
         sExtensionCheck[4] = '\0';
   }

      // If the extension check is not equal to ".TXT" or the name supplied is 4 characters or less in length
      if (((strlen(sInputChar) > 4) && (strcmp(sExtensionCheck, ".TXT")!=0))||(strlen(sInputChar) <= 4)) {
         strcpy(sOutputFileName, sInputChar);
         strcat(sOutputFileName, ".TXT");
         sim_fprintf(stdout, "\nExtension '.TXT' was added to the end of the supplied filename.\n");
         if (wInputOutputType == 1 ) {
            sim_fprintf(stdout, "Press 'Enter' to proceed");
            getc(stdin);
         }
      } else {
         strcpy(sOutputFileName, sInputChar);
      }
      if (wInputOutputType == 3) {
         pOutputFile = fopen(sOutputFileName, "w");
      }
   }

   bValidInput = false;
   sim_fprintf(stdout, "\nOutput Format Options:\n");
   sim_fprintf(stdout, "1 - Write the output as a comma-delimited data string.\n");
   sim_fprintf(stdout, "2 - Write the output as plain text.\n");
   sim_fprintf(stdout, "3 - Write the output in a timeline-style format.\n");
   sim_fprintf(stdout, "(Please enter 1 to 3):\n");

   while (!bValidInput) {
      fgets(sInputChar, 100, stdin);
      if ( IsPosShortInt(sInputChar) && ((atoi(sInputChar) >= 1) && (atoi(sInputChar) <= 3))) {
         wOutputFormat = atoi(sInputChar);
         bValidInput = true;
      } else {
         sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value 1 through 3:\n", sInputChar);
      }
   }

   bValidInput = false;

   try {
      pSimulator = new Smoking_Simulator( sInitiationFile,   sCessationFile,
                                          sOtherCODFile,     sCPDIntensityFile,
                                          sCPDDataFile,        ulInitPRNGSeed,
                                          ulCessPRNGSeed,       ulOthCODSeed,
                                          ulIndivRndSeed,       wOutputFormat,
                                          wCessationYear);

      if (wInputOutputType == 1) {
         sim_fprintf(stdout,"\n\n");
         pSimulator->RunSimulation(sInputFileName, sOutputFileName, true);
      } else if (wInputOutputType == 2) {
         sim_fprintf(stdout,"\n\n");
         pSimulator->RunSimulation(sInputFileName);
      } else {  // manually enter sex, race, Year of Birth

         bKeepRepeating = true;

         while (bKeepRepeating) {
            wInputRace = 0; // Only All Races is available in this iteration of program
            sim_fprintf(stdout,"\nEnter a sex value. \n(0 = Male, 1 = Female):\n");
            bValidInput = false;
            while (!bValidInput) {
               fgets(sInputChar, 1, stdin);
               if ( IsPosShortInt(sInputChar) && ((atoi(sInputChar) == 0) || (atoi(sInputChar) == 1))) {
                  wInputSex = (atoi(sInputChar));
                  bValidInput = true;
               } else {
                  sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter 0 or 1:\n", sInputChar);
               }
            }

            sim_fprintf(stdout,"\nEnter a year of birth between %d and %d:\n",pSimulator->GetMinYearOfBirth(),pSimulator->GetMaxYearOfBirth());
            bValidInput = false;
            while (!bValidInput) {
               fgets(sInputChar, 4, stdin);
               if ( IsPosShortInt(sInputChar) &&
                    ((atoi(sInputChar) >= pSimulator->GetMinYearOfBirth()) &&
                     (atoi(sInputChar) <= pSimulator->GetMaxYearOfBirth()))) {
                  wInputYOB   = atoi(sInputChar);
                  bValidInput = true;
               } else {
                  sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter a value between %d and %d:\n",
                         sInputChar,pSimulator->GetMinYearOfBirth(),pSimulator->GetMaxYearOfBirth());
               }
            }

            sim_fprintf(stdout,"\nNumber of persons to simulate for supplied values:\n");
            bValidInput = false;
            while (!bValidInput) {
               fgets(sInputChar, 100, stdin);
               if ( IsPosLongInt(sInputChar) && (atol(sInputChar) >= 1)) {
                  lNumRepetitions = atol(sInputChar);
                  bValidInput = true;
               } else {
                  sim_fprintf(stdout,"\n\"%s\" is not a valid value.\nAllowable range is 1 to %ld \nPlease enter a new value:\n", sInputChar, MAX(long));
               }
            }

            sim_fprintf(stdout,"\n");
            for (long j = 1; j <= lNumRepetitions; j++) {
               pSimulator->RunSimulationSingle(wInputRace, wInputSex, wInputYOB, pOutputFile);
               pSimulator->WriteToStream(stdout);
            }

            sim_fprintf(stdout, "\nSimulations complete for supplied input.\n1 - Perform more simulations\n2 - Quit\n:");
            bValidInput = false;
            while (!bValidInput) {
               fgets(sInputChar, 1, stdin);
               if ( IsPosShortInt(sInputChar)) {
                  wTempValue = atoi(sInputChar);
                  if ((wTempValue != 1) && (wTempValue != 2)) {
                     sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter 1 or 2:\n", sInputChar);
                  } else {
                     if (wTempValue == 2) {
                        bKeepRepeating = false;
                     }
                     bValidInput = true;
                  }
               } else {
                  sim_fprintf(stdout,"\n\"%s\" - Invalid Input.\nPlease enter 1 or 2:\n", sInputChar);
               }
            }

         } // while(bKeepRepeating)
      }
      sim_fprintf(stdout,"\nSimulations complete\nPress \"Enter\" to close this window\n");
      getc(stdin);
      }
   catch (SimException ex) {
      sim_fprintf(stdout,"\nInternal error occurred\n");
      sim_fprintf(stdout,"Error : %s\n",ex.GetError());
      getc(stdin);
   } catch (...) {
      sim_fprintf(stdout,"\nUnknown Error Occurred\n");
      getc(stdin);
   }

   if (pOutputFile != 0) {
      fclose(pOutputFile);
   }

   delete pSimulator;
   delete [] sInitiationFile; delete [] sCessationFile; delete [] sOtherCODFile; delete [] sCPDIntensityFile; delete [] sCPDDataFile;
}

// Runs the application using a single data file containing all necessary information.
// The output from this run is written in XML-style tags
int RunWebVersion(const char * sInputFileName)
{
   bool bRunApp = true,
        bHaveVectorValues = false,
        bUseNumReps = false;

   char sInputLine[1000],
        *sErrorFile      = 0,
        *sRNGStrategy    = 0,
        *sInputBuffer    = 0,
        *sFILE_InitProb  = 0, // Datafile - Initiation Probabilities
        *sFILE_CessProb  = 0, // Datafile - Cessation Probabilities
        *sFILE_OCDProb   = 0, // Datafile - Life Table (Probability of Dying from Cause other than Lung Cancer)
        *sFILE_Quintiles = 0, // Datafile - Smoking Intensity Quintile Placement Probabilites
        *sFILE_CPDData   = 0, // Datafile - Smoking Intensity - Cigarettes per Day by Quintile
        *sSEED_Init      = 0, // Seed - For PRNG that generates Initiation Probabilities
        *sSEED_Cess      = 0, // Seed - For PRNG that generates Cessation Probabilities
        *sSEED_OCD       = 0, // Seed - For PRNG that generates Death from Other OCD probabilities
        *sSEED_Misc      = 0, // Seed - For PRNG that generates miscellaneous random numbers that are needed for 1 time use for a person
        *sOutputFile     = 0,
        *sImmediateCess  = 0, // Immediate Cessation Year, 0 = do not do immediate cessation
        *sPARAM_Sex      = 0, // Run Parameter - Sex
        *sPARAM_Race     = 0, // Run Parameter - Race
        *sPARAM_YOB      = 0, // Run Parameter - Year of Birth
        *sPARAM_NumReps  = 0, // Run Parameter - Number of time to repeat current set of parameters
        sVecValues[4][20];

   FILE *pInputFile   = 0,
        *pOutStream   = 0,
        *pErrorStream = 0;

   int   iCurrIndex,
         iIndexLength,
         iReturnValue,
         iStringLength,
         i;

   long  lNumReps,
         lSeed_Init,
         lSeed_Cess,
         lSeed_OCD,
         lSeed_Misc,
         j;
   unsigned long rngStreamSeed[6] =  {12345, 12345, 12345, 12345, 12345, 12345};

   short wValuesPerParam[4],
         wMaxNumPerParam,
         wCessationYear;

   Smoking_Simulator *pSimulator = 0;

   gInputFileName = sInputFileName;

   pInputFile = fopen(sInputFileName,"r");
   
   if (pInputFile == NULL) {
      sim_fprintf(stdout, "The specified input file does not exist or could not be opened.\n");
      bRunApp = false;
   }
   
   if (bRunApp) {

      while (fgets(sInputLine, 1000, pInputFile) != NULL) {

         sInputBuffer = new char[strlen(sInputLine) + 1];
         strcpy(sInputBuffer, sInputLine);
         iIndexLength = 0;
         iStringLength = strlen(sInputBuffer);

         if (strstr(sInputBuffer, "\r") != NULL) {
            iStringLength--;
         }

         if (strstr(sInputBuffer,"\n")!=NULL) {
            iStringLength--;
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_INIT=", strlen("SEED_INIT=")) == 0) {
            iIndexLength = strlen("SEED_INIT=");
            sSEED_Init = new char[(iStringLength - iIndexLength) + 1];
            iCurrIndex = 0;
            for ( i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength] != ' ') {
                  sSEED_Init[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Init[iCurrIndex] = '\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_CESS=", strlen("SEED_CESS=")) == 0) {
            iIndexLength = strlen("SEED_CESS=");
            sSEED_Cess = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Cess[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Cess[iCurrIndex] = '\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_OCD=", strlen("SEED_OCD=")) == 0) {
            iIndexLength = strlen("SEED_OCD=");
            sSEED_OCD = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_OCD[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_OCD[iCurrIndex]='\0';
         }

         if (strncmp(Str_toupper(sInputBuffer), "SEED_MISC=", strlen("SEED_MISC=")) == 0) {
            iIndexLength = strlen("SEED_MISC=");
            sSEED_Misc = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sSEED_Misc[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sSEED_Misc[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "SEX=") != NULL) {
            iIndexLength = strlen("SEX=");
            sPARAM_Sex = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for ( i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength] != ' ') {
                  sPARAM_Sex[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_Sex[iCurrIndex]='\0';
            if (strchr(sPARAM_Sex, ',') != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "RACE=") != NULL) {
            iIndexLength = strlen("RACE=");
            sPARAM_Race = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sPARAM_Race[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_Race[iCurrIndex]='\0';
            if (strchr(sPARAM_Race, ',') != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "YOB=") != NULL) {
            iIndexLength = strlen("YOB=");
            sPARAM_YOB = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sPARAM_YOB[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_YOB[iCurrIndex]='\0';
            if (strchr(sPARAM_YOB, ',') != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "REPEAT=") != NULL) {
            iIndexLength = strlen("REPEAT=");
            sPARAM_NumReps = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputBuffer[i + iIndexLength]!= ' ') {
                  sPARAM_NumReps[iCurrIndex] = sInputBuffer[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sPARAM_NumReps[iCurrIndex]='\0';
            if (strchr(sPARAM_NumReps, ',') != NULL)
               bHaveVectorValues = true;
         }

         if (strstr(Str_toupper(sInputBuffer), "INIT_PROB=") != NULL) {
            iIndexLength = strlen("INIT_PROB=");
            sFILE_InitProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_InitProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_InitProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "CESS_PROB=") != NULL) {
            iIndexLength = strlen("CESS_PROB=");
            sFILE_CessProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_CessProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_CessProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "OCD_PROB=") != NULL) {
            iIndexLength = strlen("OCD_PROB=");
            sFILE_OCDProb = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_OCDProb[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_OCDProb[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "CPD_QUINTILES=") != NULL) {
            iIndexLength = strlen("CPD_QUINTILES=");
            sFILE_Quintiles = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_Quintiles[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_Quintiles[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "CPD_DATA=") != NULL) {
            iIndexLength = strlen("CPD_DATA=");
            sFILE_CPDData = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sFILE_CPDData[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sFILE_CPDData[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "OUTPUTFILE=") != NULL) {
            iIndexLength = strlen("OUTPUTFILE=");
            sOutputFile = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sOutputFile[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sOutputFile[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "ERRORFILE=") != NULL) {
            iIndexLength = strlen("ERRORFILE=");
            sErrorFile = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sErrorFile[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sErrorFile[iCurrIndex]='\0';
         }
         
         if (strstr(Str_toupper(sInputBuffer), "RNGSTRATEGY=") != NULL) {
            iIndexLength = strlen("RNGSTRATEGY=");
            sRNGStrategy = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sRNGStrategy[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sRNGStrategy[iCurrIndex]='\0';
         }

         // Note: RngStream's default seed is an array of 6 integers {12345,12345,12345,12345,12345,12345}
         // It is fine to use the default seed, but also good to allow the user to specify their own seed
         // Only one seed is requred for RngStream because it uses substreams to generate multiple (IID) streams
         if (strstr(Str_toupper(sInputBuffer), "RNGSTREAM_SEED=") != NULL) {
            iIndexLength = strlen("RNGSTREAM_SEED=");
            char* seedString = new char[(iStringLength - iIndexLength) + 1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength] != ' ') {
                     seedString[iCurrIndex] = sInputLine[i + iIndexLength];
                     iCurrIndex++;
               }
            }
            seedString[iCurrIndex] = '\0';

            // Parse the comma-delimited string into an array of 6 unsigned long integers
            char* token = strtok(seedString, ",");
            int index = 0;
            while (token != NULL && index < 6) {
               rngStreamSeed[index] = atoi(token);
               token = strtok(NULL, ",");
               index++;
            }

            // Ensure we have exactly 6 values
            if (index != 6) {
               sim_fprintf_stderr("Error: RNGSTREAM_SEED must contain exactly 6 comma-delimited integers.\n");
               // TODO we could/should also check for valid values here (see RngStream documentation)
               // Handle error appropriately (e.g., set a flag, exit, etc.)
            }

            delete[] seedString;
         }

         if (strstr(Str_toupper(sInputBuffer), "IMMEDIATECESS=") != NULL) {
            iIndexLength = strlen("IMMEDIATECESS=");
            sImmediateCess = new char[(iStringLength - iIndexLength)+1];
            iCurrIndex = 0;
            for (i = 0; i < (iStringLength - iIndexLength); i++) {
               if (sInputLine[i + iIndexLength]!= ' ') {
                  sImmediateCess[iCurrIndex] = sInputLine[i + iIndexLength];
                  iCurrIndex++;
               }
            }
            sErrorFile[iCurrIndex]='\0';
         }

         if (strstr(Str_toupper(sInputBuffer), "NOTAGS") != NULL) {
            gWithHoldTags = true;
         }

         delete [] sInputBuffer;
         sInputBuffer = 0;
      } // end While

      fclose(pInputFile);

      // Check for the error file string, open it if it exists, otherwise, open the default error file
      if (sErrorFile == NULL) {
         sim_fprintf(stdout,"Name for Error log file was not found in input file: %s", sInputFileName);
         bRunApp = false;
      } else {
         pErrorStream = fopen(sErrorFile,"w");
      }

      if (sRNGStrategy == NULL) {   
         //sim_fprintf(stdout,"Using default random number strategy of Mersenne Twister because none was specified in input file: %s\n", sInputFileName);
         const char* sRNGStrategy = "MersenneTwister";
      }
      
      else if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
         //sim_fprintf(stdout,"Using Mersenne Twister random number generator strategy.\n");
      }
      else if (strcmp(sRNGStrategy, "RngStream") == 0) {
         //sim_fprintf(stdout,"Using RngStream random number generator strategy.\n");
      }
      else {
         sim_fprintf_stderr("The specified RNG strategy is invalid: %s\n", sRNGStrategy);
         bRunApp = false;
      }

      if (bRunApp && pErrorStream == NULL) {
         sim_fprintf_stderr("Specified error file: %s could not be opened for writing.\n", sErrorFile);
         bRunApp = false;
      }
   } 

   if (bRunApp) {
      // Make sure all necessary values were received
        if (strcmp(sRNGStrategy, "MersenneTwister") == 0) {
         // Check MT Seeds
         if (sSEED_Init == NULL) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nSeed for Initiation PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sInputFileName);
            bRunApp = false;
         } else if (!IsValidSeed(sSEED_Init)) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nInvalid Initiation PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Init,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_Cess == NULL) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nSeed for Cessation PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sInputFileName);
            bRunApp = false;
         } else if (!IsValidSeed(sSEED_Cess)) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nInvalid Cessation PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Cess,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_OCD == NULL) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nSeed for OCD PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sInputFileName);
            bRunApp = false;
         } else if (!IsValidSeed(sSEED_OCD)) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nInvalid OCD PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_OCD,sInputFileName);
            bRunApp = false;
         }
         if (sSEED_Misc == NULL) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nSeed for Miscellaneous PRNG was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sInputFileName);
            bRunApp = false;
         } else if (!IsValidSeed(sSEED_Misc)) {
            sim_fprintf(pErrorStream,"\n<ERROR>\nInvalid Miscellaneous PRNG Seed: %s found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                  sSEED_Misc,sInputFileName);
            bRunApp = false;
         }
      }

      // Check Files
      if (sFILE_InitProb == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nInitiation Probabilities file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_CessProb == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nCessation Probabilities file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_OCDProb == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nOCD Probabilities file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sFILE_CPDData == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nCPD Data file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sOutputFile == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nOutput file was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }

      // Check parameters
      if (sPARAM_Sex == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nSex value(s) was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sPARAM_Race == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nRace value(s) was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }
      if (sPARAM_YOB == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nYear of Birth value(s) was not found in input file: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sInputFileName);
         bRunApp = false;
      }

      // Check the optional sPARAM_NumReps value if we are not using a vector
      if (sPARAM_NumReps != NULL && !bHaveVectorValues && !IsValidNumReps(sPARAM_NumReps)) {
         sim_fprintf(pErrorStream, "\n<ERROR>\nInvalid Number of Repetitions: %s,\n Value must be a positive integer with a max value of %d.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",
                 sPARAM_NumReps, MAX_NUM_REPS);
         bRunApp = false;
      } 
      else if (sPARAM_NumReps!=NULL && !bHaveVectorValues) {
         bUseNumReps = true;
         lNumReps    = atol(sPARAM_NumReps);
      } else if (sPARAM_NumReps!=NULL && bHaveVectorValues) {
         bUseNumReps = true;
      } else {
         bUseNumReps = false;
      }

   }  // end if (bRunApp)


   if (bRunApp) { 
      // Still can run, try to open the output file
      pOutStream   = fopen(sOutputFile,"w");
      if (pOutStream == NULL) {
         sim_fprintf(pErrorStream,"\n<ERROR>\nSupplied Output file: %s, could not be opened for writing.\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n",sOutputFile);
         bRunApp = false;
      }
   }

   // Parse the Vector values if applicable
   if (bRunApp && bHaveVectorValues) {
      wValuesPerParam[0] = CountVectorValues(sPARAM_Race);
      wValuesPerParam[1] = CountVectorValues(sPARAM_Sex);
      wValuesPerParam[2] = CountVectorValues(sPARAM_YOB);
      wValuesPerParam[3] = CountVectorValues(sPARAM_NumReps);
      wMaxNumPerParam = 1;
      for (i=0; i < 4; i++) {
      	if ((wValuesPerParam[i] > wMaxNumPerParam) && 
             (wMaxNumPerParam > 1) || 
             (wMaxNumPerParam > 1 && 
              (wValuesPerParam[i] != wMaxNumPerParam && wValuesPerParam[i] > 1))) {

            bRunApp = false;
            sim_fprintf(pErrorStream, "\n<ERROR>");
            sim_fprintf(pErrorStream, "\nInvalid use of vector values in the input file.");
            sim_fprintf(pErrorStream, "\nIf vector values are used for more than 1 variable,");
            sim_fprintf(pErrorStream, "\nthe same number of values must be supplied for each variable.");
            sim_fprintf(pErrorStream, "\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n");
	      } else if (wValuesPerParam[i] > wMaxNumPerParam) {
	         wMaxNumPerParam = wValuesPerParam[i];
	      }
	   } // end for
   } // end if (bRunApp && bHaveVectorValues)

   if (bRunApp) {
      lSeed_Init = atol(sSEED_Init);
      if (lSeed_Init == -1)
         lSeed_Init = time(0);
      lSeed_Cess = atol(sSEED_Cess);
      if (lSeed_Cess == -1)
         lSeed_Cess = time(0);
      lSeed_OCD = atol(sSEED_OCD);
      if (lSeed_OCD == -1)
         lSeed_OCD = time(0);
      lSeed_Misc = atol(sSEED_Misc);
      if (lSeed_Misc == -1)
         lSeed_Misc = time(0);

      if (sImmediateCess == NULL) {
         wCessationYear = 0;
      } else {
         wCessationYear = (short) atoi(sImmediateCess);
      }

      try {
         short wOutputType = 1; // OUT_DataOnly=1
         pSimulator = new Smoking_Simulator(sFILE_InitProb,  sFILE_CessProb,
                                            sFILE_OCDProb,   sFILE_Quintiles,
                                            sFILE_CPDData,   wOutputType,
                                            wCessationYear);

         // You may use additional strategies of class RNG_Strategy and instantiate them here without changing the smoking_sim class
         if (strcmp(sRNGStrategy, "RngStream") == 0){
            pSimulator->setRNGStrategy(new RngStreamRNG(rngStreamSeed));
         }
         else if (strcmp(sRNGStrategy, "MersenneTwister") == 0){
            pSimulator->setRNGStrategy(new MersenneTwisterRNG(lSeed_Init, lSeed_Cess, lSeed_OCD, lSeed_Misc));
         }
         else {
            sim_fprintf(pErrorStream, "\n<ERROR>\nInvalid RNG Strategy: %s\n</ERROR>\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>\n", sRNGStrategy);
            bRunApp = false;
         }

         // Measure & build input data string
         if (!gWithHoldTags) {
            WriteRunInfoTag(pOutStream,VERSION_NUM, sSEED_Init, sSEED_Cess, sSEED_OCD,
                         sSEED_Misc, sImmediateCess, sFILE_InitProb, sFILE_CessProb, sFILE_OCDProb,
                         sFILE_Quintiles, sFILE_CPDData, sOutputFile, sErrorFile, sRNGStrategy);
         }

      } catch (SimException ex) {
	      sim_fprintf(pErrorStream, "\n<ERROR>\n%s\n</ERROR>\n", ex.GetError());
         sim_fprintf(pErrorStream, "<CALLPATH>\n%s\n</CALLPATH>", ex.GetCallPath());
         bRunApp = (ex.GetType() == SimException::NON_FATAL);
      }
   }

   if (bRunApp) {
      if (bHaveVectorValues) {
         if (wValuesPerParam[0] <= 1)
            strcpy(sVecValues[0], sPARAM_Race);
         if (wValuesPerParam[1] <= 1)
            strcpy(sVecValues[1], sPARAM_Sex);
         if (wValuesPerParam[2] <= 1)
            strcpy(sVecValues[2], sPARAM_YOB);
         if (bUseNumReps && wValuesPerParam[3] <= 1)
            strcpy(sVecValues[3], sPARAM_NumReps);
         else if (!bUseNumReps)
            strcpy(sVecValues[3], "\0");

         for (i = 0; i < wMaxNumPerParam && bRunApp; i++) {
            if (wValuesPerParam[0] > 1)
               LoadValue(sVecValues[0], sPARAM_Race, i);
            if (wValuesPerParam[1] > 1)
               LoadValue(sVecValues[1], sPARAM_Sex, i);
            if (wValuesPerParam[2] > 1)
               LoadValue(sVecValues[2], sPARAM_YOB, i);
            if (bUseNumReps && wValuesPerParam[3] > 1)
	            LoadValue(sVecValues[3], sPARAM_NumReps, i);

            if (!gWithHoldTags) {
               sim_fprintf(pOutStream, "<SIMULATION>\n");
            }
            WriteInputTag(pOutStream, sVecValues[0], sVecValues[1], sVecValues[2], sVecValues[3]);
            if (!gWithHoldTags) {
               sim_fprintf(pOutStream, "<RUN>\n");
            }

            if (bUseNumReps && !IsValidNumReps(sVecValues[3])) {
	            sim_fprintf(pErrorStream, "\n<ERROR>\nInvalid Number of Repetitions: %s, \n Value must be a positive integer with a max value of %d.\n</ERROR>", sVecValues[3], MAX_NUM_REPS);
               sim_fprintf(pErrorStream, "\n<CALLPATH>\nMain:RunWebVersion()\n</CALLPATH>");
               sim_fprintf(pOutStream, "<RESULT>\nERROR\n</RESULT>\n</RUN>\n</SIMULATION>\n");
            } else if (bUseNumReps) {
               lNumReps = atol(sVecValues[3]);
               for (j=0; j<lNumReps && bRunApp;j++) {
                  try {
                     pSimulator->RunSimulationSingle(atoi(sVecValues[0]), atoi(sVecValues[1]),
                                               atoi(sVecValues[2]), pOutStream);
                  } catch (SimException ex) {
            	      sim_fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
                     sim_fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
                     bRunApp = (ex.GetType() == SimException::NON_FATAL);
                     sim_fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
                  }
               }
               sim_fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
            } else {
               try {
                  pSimulator->RunSimulationSingle(atoi(sVecValues[0]), atoi(sVecValues[1]),
                                            atoi(sVecValues[2]), pOutStream);
               } catch (SimException ex) {
           	      sim_fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
                  sim_fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
                  bRunApp = (ex.GetType() == SimException::NON_FATAL);
                  sim_fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
               }
               if (!gWithHoldTags) 
                  sim_fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
            }
	      }  
      } else if (bUseNumReps) {
         if (!gWithHoldTags) 
            sim_fprintf(pOutStream,"<SIMULATION>\n");
         WriteInputTag(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
         if (!gWithHoldTags) 
            sim_fprintf(pOutStream,"<RUN>\n");
         for (j=0; j<lNumReps && bRunApp; j++) {
            try {
               pSimulator->RunSimulationSingle(atoi(sPARAM_Race),atoi(sPARAM_Sex),atoi(sPARAM_YOB),pOutStream);
            } catch(SimException ex) {
        	      sim_fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
               sim_fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
               bRunApp = (ex.GetType() == SimException::NON_FATAL);
               sim_fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
            }
         }
         if (!gWithHoldTags) 
            sim_fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
      } else {
         try {
            sim_fprintf(pOutStream,"<SIMULATION>\n");
            WriteInputTag(pOutStream,sPARAM_Race,sPARAM_Sex,sPARAM_YOB,sPARAM_NumReps);
            sim_fprintf(pOutStream,"<RUN>\n");
            pSimulator->RunSimulationSingle(atoi(sPARAM_Race),atoi(sPARAM_Sex),atoi(sPARAM_YOB),pOutStream);
            if (!gWithHoldTags)
               sim_fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
         } catch (SimException ex) {
            sim_fprintf(pErrorStream,"\n<ERROR>\n%s\n</ERROR>\n",ex.GetError());
            sim_fprintf(pErrorStream,"<CALLPATH>\n%s\n</CALLPATH>",ex.GetCallPath());
            bRunApp = (ex.GetType() == SimException::NON_FATAL);
            sim_fprintf(pOutStream,"<RESULT>\nERROR\n</RESULT>\n");
            sim_fprintf(pOutStream,"</RUN>\n</SIMULATION>\n");
         }
      }
   }

   if (pOutStream != NULL)
      fclose(pOutStream);

   if (pErrorStream != NULL) {
      fclose(pErrorStream);
      std::ifstream errorFile(sErrorFile, std::ios::binary | std::ios::ate);
      long fileSize = errorFile.tellg();
      errorFile.close();
      if (fileSize == 0) {
         remove(sErrorFile);
      }
   }

   if (bRunApp)
      iReturnValue = 1;
   else
      iReturnValue = 0;

   delete [] sErrorFile;
   delete [] sRNGStrategy;
   delete [] sInputBuffer;
   delete [] sFILE_InitProb;
   delete [] sFILE_CessProb;
   delete [] sFILE_OCDProb;
   delete [] sFILE_Quintiles;
   delete [] sFILE_CPDData;
   delete [] sSEED_Init;
   delete [] sSEED_Cess;
   delete [] sSEED_OCD;
   delete [] sSEED_Misc;
   delete [] sOutputFile;
   delete [] sImmediateCess;
   delete [] sPARAM_Sex;
   delete [] sPARAM_Race;
   delete [] sPARAM_YOB;
   delete [] sPARAM_NumReps;
   delete    pSimulator;

   return iReturnValue;
}

//---------------------------------------------------------------------------
char* Str_toupper(char *s) {
	char* p = s;
	while (*s) {
		*s = toupper(*s);
		s++;
	}
	return p;
}	

char* Str_tolower(char *s) {
	char* p = s;
	while (*s) {
		*s = tolower(*s);
		s++;
	}
	return p;
}	

// Print a usage message to STDERR
void Usage(void) {
   sim_simple_stdout("Usage:\n");
   sim_simple_stdout(" Smoking_Initiation\n");
   sim_simple_stdout("        Runs a user interface version of program.\n\n");
   sim_simple_stdout("Or\n\n");
   sim_simple_stdout(" Smoking_Initiation DATA_DIR INIT_SEED CESS_SEED OTH_COD_SEED INPUT_FILE OUTPUT_FILE OUTPUT_TYPE CESS_YEAR\n");
   sim_simple_stdout("\nOr\n\n");
   sim_simple_stdout(" Smoking_Initiation INIT_SEED CESS_SEED OTH_COD_SEED INPUT_FILE OUTPUT_FILE OUTPUT_TYPE CESS_YEAR\n");
   sim_simple_stdout(" Where:\n");
   sim_simple_stdout("    DATA_DIR     - Directory that contains the input files used by the application \n");
   sim_simple_stdout("    INIT_SEED    - An integer seed for the Initiation Probability PRNG (>= 0)\n");
   sim_simple_stdout("    CESS_SEED    - An integer seed for the Cessation Probability PRNG (>= 0)\n");
   sim_simple_stdout("    OTH_COD_SEED - An integer seed for the Other Cause of Death Probability PRNG (>= 0)\n");
   sim_simple_stdout("    INDIV_SEED   - An integer seed for the PRNG that will be used for defining characteristics of the individual(>= 0)\n");
   sim_simple_stdout("    INPUT_FILE   - Name of file containing co-variates to use in simulation\n");
   sim_simple_stdout("    OUTPUT_FILE  - Path where output will be written\n");
   sim_simple_stdout("    OUTPUT_TYPE  - Format for output file (1=Data, 2=Text, 3=Timeline)\n");
   sim_simple_stdout("    CESS_YEAR    - 4-digit Year Value. All smokers will stop smoking on January 1st of year provided.\nEnter a value of '0' to disable the immediate cessation option.\n");
   sim_simple_stdout(" Press any key to close window");
   getc(stdin);
}


// Validate the parameters necessary to run application
bool ValidateParameters(char* sDataFileDir, 
                        char* sInitiationSeed, char* sCessationSeed, char* sOtherCODSeed, char* sIndivRndSeed, 
                        char* sInputFile, char* sOutputFile,
                        char* sOutputType, char* sImmediateCess, char* sErrorMessage) {

   bool bReturnValue = true;
   int i, iCurrIndex;
   char *sTestDirStr;
	FILE *pTestInputStream  = 0;

   sTestDirStr = AssignFilename(sDataFileDir, INITIATION_DATA_FILE);
	pTestInputStream = fopen(sTestDirStr, "r");

	if (pTestInputStream == NULL) {
		sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Input File %s could not be opened for reading.\n", sTestDirStr);
		bReturnValue = false;
  	}
	if (pTestInputStream != NULL) {
      fclose(pTestInputStream);
   }
   if (bReturnValue) {
      bReturnValue = ValidateParameters(sInitiationSeed, sCessationSeed, sOtherCODSeed, sIndivRndSeed,
                                        sInputFile, sOutputFile, 
                                        sOutputType, sImmediateCess, sErrorMessage);
   }
   return bReturnValue;
}


// Validate the parameters necessary to run application
bool ValidateParameters(char* sInitiationSeed, char* sCessationSeed, char* sOtherCODSeed, char* sIndivRndSeed,
                        char* sInputFile, char* sOutputFile,
                        char* sOutputType, char* sImmediateCess, char* sErrorMessage) {

	FILE *pTestInputStream  = 0,
		  *pTestOutputStream = 0;
	bool bReturnValue = true;

	if (!IsPosLongInt(sInitiationSeed)) {
		sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid Seed %s for Initiation Probability PRNG.\nValid Range id 0 to %ld.\n", 
         sInitiationSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosLongInt(sCessationSeed)) {
		sim_snprintf(sErrorMessage, sizeof(sErrorMessage),"Invalid Seed %s for Cessation Probability PRNG.\nValid Range id 0 to %ld.\n", 
         sCessationSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosLongInt(sOtherCODSeed)) {
		sim_snprintf(sErrorMessage, sizeof(sErrorMessage),"Invalid Seed %s for Other Cause of Death Probability PRNG.\nValid Range id 0 to %ld.\n", 
         sOtherCODSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosLongInt(sIndivRndSeed)) {
		sim_snprintf(sErrorMessage, sizeof(sErrorMessage),"Invalid Seed %s for Indivdual's Random Numbers PRNG.\nValid Range id 0 to %ld.\n", 
         sIndivRndSeed, MAX(long));
		bReturnValue = false;
  	} else if (!IsPosShortInt(sImmediateCess) ||
              ((atoi(sImmediateCess) != 0) &&
               (atoi(sImmediateCess) < wMIN_IMMEDIATE_CESSATION_YEAR || 
                atoi(sImmediateCess) > wSIM_CUTOFF_YEAR))) {
      sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value %s for Immediate Cessation Year. \nValid values are 0, %d-%d.\n", 
              sImmediateCess, wMIN_IMMEDIATE_CESSATION_YEAR, wSIM_CUTOFF_YEAR);
      bReturnValue = false;
   } else if (!IsPosShortInt(sOutputType) ||
           (atoi(sOutputType) < (short)Smoking_Simulator::OUT_DataOnly) ||
           (atoi(sOutputType) >= (short)Smoking_Simulator::OUT_Uninitialized)) {
      sim_snprintf(sErrorMessage, sizeof(sErrorMessage),"Invalid Output Type: %d\nValid values are %d to ?.\n",
              (short)Smoking_Simulator::OUT_DataOnly, ((short)Smoking_Simulator::OUT_Uninitialized-1));
		bReturnValue = false;
  	}

	// Make sure input and output files can be opened for reading/writing respectively
	if (bReturnValue) {
		pTestInputStream  = fopen(sInputFile, "r");
		pTestOutputStream = fopen(sOutputFile, "w");
		if (pTestInputStream == NULL) {
			sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Input File %s could not be opened for reading.\n", sInputFile);
			bReturnValue = false;
	  	}
      if (pTestInputStream  != NULL) {
         fclose(pTestInputStream);
      }
		if (bReturnValue && pTestOutputStream == NULL) {
			sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Output File %s could not be opened for writing.\n", sOutputFile);
			bReturnValue = false;
	  	}
		if (pTestOutputStream != NULL) {
         fclose(pTestOutputStream);
      }
  	}
	return bReturnValue;
}

//Writes out tagged information about the program to pOutStream
void WriteRunInfoTag(FILE* pOutStream, std::string sVersion, const char* sInitSeed,
                     const char* sCessSeed, const char* sOCDSeed, const char* sMiscSeed,
                     const char* sImmediateCessYear, const char* sInitFile, const char* sCessFile,
                     const char* sOCDProbFile, const char* sQuintilesFile, const char* sCPDDataFile,
                     const char* sOutputFile, const char* sErrorFile, const char* sRNGStrategy) {

   if (pOutStream == NULL)
      throw SimException("WriteRunInfoTag()::ERROR","Output stream is not initialized.\n");

   sim_fprintf(pOutStream,"<RUNINFO>\n");
   sim_fprintf(pOutStream,"<VERSIONINFO>\n%s\n</VERSIONINFO>\n", VERSIONJSON.c_str());
   sim_fprintf(pOutStream,"<SEEDS>\n<INIT_PRNG_SEED>\n%s\n</INIT_PRNG_SEED>\n", sInitSeed);
   sim_fprintf(pOutStream,"<CESS_PRNG_SEED>\n%s\n</CESS_PRNG_SEED>\n", sCessSeed);
   sim_fprintf(pOutStream,"<OCD_PRNG_SEED>\n%s\n</OCD_PRNG_SEED>\n", sOCDSeed);
   sim_fprintf(pOutStream,"<MISC_PRNG_SEED>\n%s\n</MISC_PRNG_SEED>\n</SEEDS>\n", sMiscSeed);
   sim_fprintf(pOutStream,"<DATAFILES>\n");
   sim_fprintf(pOutStream,"<INPUT_FILE>\n%s\n</INPUT_FILE>\n", gInputFileName.c_str());
   sim_fprintf(pOutStream,"<INITIATION>\n%s\n</INITIATION>\n", sInitFile);
   sim_fprintf(pOutStream,"<CESSATION>\n%s\n</CESSATION>\n", sCessFile);
   sim_fprintf(pOutStream,"<OCD>\n%s\n<OCD>\n", sOCDProbFile);
   sim_fprintf(pOutStream,"<CIG_PER_DAY>\n%s\n</CIG_PER_DAY>\n</DATAFILES>\n", sCPDDataFile);
   sim_fprintf(pOutStream,"<OUTFILES>\n<OUTPUT>\n%s\n</OUTPUT>\n", sOutputFile);
   sim_fprintf(pOutStream,"<ERRORS>\n%s\n</ERRORS>\n</OUTFILES>\n", sErrorFile);
   sim_fprintf(pOutStream,"<OPTIONS>\n<CESSATION_YR>\n%s\n</CESSATION_YR>\n", sImmediateCessYear);
   sim_fprintf(pOutStream,"<RNGSTRATEGY>\n%s\n</RNGSTRATEGY>\n", sRNGStrategy);
   sim_fprintf(pOutStream,"</OPTIONS>\n</RUNINFO>\n");

}

//Writes out tagged information about the current run to pOutStream
void WriteInputTag(FILE* pOutStream, char* sRace, char* sSex, const char* sYearOfBirth, const char* sNumReps) {

   int iSex, iRace;

   try {
      if (pOutStream == NULL) {
         throw SimException("ERROR","Output stream is not initialized.\n");
      }

      iSex = atoi(sSex);
      iRace = atoi(sRace);

      if (!gWithHoldTags) {
         sim_fprintf(pOutStream, "<INPUTS>\n");

         if (iRace >= 0 && iRace < Smoking_Simulator::NUM_RACES) {
            sim_fprintf(pOutStream, "<RACE>\n%s\n</RACE>\n", sRACE_LABELS[iRace]);
         } else {
            sim_fprintf(pOutStream, "<RACE>\n%d\n</RACE>\n", iRace);
         }

         if (iSex >= 0 && iSex < Smoking_Simulator::NUM_SEXES) {
            sim_fprintf(pOutStream,"<SEX>\n%s\n</SEX>\n", sSEX_LABELS[iSex]);
         } else {
            sim_fprintf(pOutStream,"<SEX>\n%d\n</SEX>\n", iSex);
         }

         sim_fprintf(pOutStream,"<YOB>\n%s\n</YOB>\n",sYearOfBirth);
         if (sNumReps != NULL && (strcmp(sNumReps,"\0") != 0)) {
            sim_fprintf(pOutStream,"<REPEAT>\n%s\n</REPEAT>\n", sNumReps);
   	   }
         sim_fprintf(pOutStream,"</INPUTS>\n");
      }
   } catch (SimException ex) {
      ex.AddCallPath("WriteInputTag(FILE*,char*...)");
      throw ex;
   }
}

void ModifyCutoffYear(char* newCutoff) {
   wSIM_CUTOFF_YEAR = min(atoi(newCutoff), wSIM_CUTOFF_YEAR);
}

short min(short first, short second){
   if (first < second) {
      return first;
   } else {
      return second;
   }
}

// Create a data file simulating  sNumToSimulate people for each race/sex/year of birth
// Assume 20 cigarettes per day for current smokers
// All seeds have the value of 0
// This routine is used during the applications development to test the results
// It SHOULD NOT be used by/with the CISNET models
bool CreateDataFile(const char *sNumToSimulate, const char* sOutFileName, char* sErrorMessage) {

   bool bReturnValue = true;
   Smoking_Simulator *pSimulator = 0;
   FILE *pOutputFile = 0;
   short wNumToSimulate, i, j, k;
   long l, lNumToSimulate;

   if (IsPosLongInt(sNumToSimulate)) {
      lNumToSimulate = atol(sNumToSimulate);
      try {
         short wCessationYear = 0;
         pSimulator = new Smoking_Simulator(INITIATION_DATA_FILE, CESSATION_DATA_FILE,
                                            OTHER_COD_DATA_FILE,  CPD_INTENSITY_PROBS,
                                            CPD_DATA_FILE,        0,
                                            0,                    0,
                                            0,                    Smoking_Simulator::OUT_DataOnly,
                                            wCessationYear);

         pOutputFile = fopen(sOutFileName, "w");
         for (i = 1; i <= pSimulator->GetNumRaceValues(); i++) {
            for (j = 1; j<= pSimulator->GetNumSexValues(); j++) {
               for (k = pSimulator->GetMinYearOfBirth(); k <= pSimulator->GetMaxYearOfBirth(); k++) {
                  for (l = 0; l < lNumToSimulate; l++) {
                     pSimulator->RunSimulationSingle( i, j, k, pOutputFile);
                  }
                  printf("%d %d %d\n",i,j,k);
               }
            }
         }
         fclose(pOutputFile);

      } catch (SimException ex) {
         sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "%s", ex.GetError());
         delete pSimulator; pSimulator = 0;
		   bReturnValue = false;
   	} catch (...) {
         sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Unknown Error Occurred\n");
         delete pSimulator; pSimulator = 0;
		   bReturnValue = false;
   	}
   } else {
      sim_snprintf(sErrorMessage, sizeof(sErrorMessage), "Invalid value: %s, supplied for number of simulations to run.\n", sNumToSimulate);
      bReturnValue = false;
   }

	delete pSimulator;
	return bReturnValue;
}

void PropagateVersionInformation() {
   std::ifstream infile(VERSION_FILE);
   std::string fileData((std::istreambuf_iterator<char> (infile)), std::istreambuf_iterator<char> ());
   infile.close();
   VERSIONJSON = fileData;
}
