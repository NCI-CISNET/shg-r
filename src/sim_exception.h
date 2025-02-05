//CISNET (www.cisnet.cancer.gov)
//Colorectal Base Case Group
//First Screening Age Simulation Application
//Class defines the simulaor exceptions thrown by the application when errors occur.
//------------------------------------------------------------------------------
//File: sim_exception.h
//Author: Martin Krapcho
//E-Mail: KrapchoM@imsweb.com
//NCI Contacts: Kevin Dodd, Barnali Das
//------------------------------------------------------------------------------
//Version 1.0.0
//------------------------------------------------------------------------------

#ifndef _SIMEXCEPTION_H
#define _SIMEXCEPTION_H

#include <cstdlib>

//Simulator Exception Class
//This class willl handle file errors and other errors that might dictate the
// necessity to close the main program.
class SimException
{
   public:
      enum eExceptType {FATAL=0, NON_FATAL, NUM_VALUES};
   private:
      char        gsCallPath[1001];
      char        gsError[1001];
      eExceptType geType;
   public:
      SimException(const char* sErrorCallPath, const char* sThrownError,
                   eExceptType eType=FATAL);
      void        AddCallPath(const char* sAddCallPath);
      const char* GetCallPath() const {return gsCallPath;};
      const char* GetError() const {return gsError;};
      eExceptType GetType() const {return geType;};
};

#endif
