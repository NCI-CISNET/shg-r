//CISNET (www.cisnet.cancer.gov)
//Colorectal Base Case Group
//First Screening Age Simulation Application
//Class defines the simulaor exceptions thrown by the application when errors occur.
//------------------------------------------------------------------------------
//File: sim_exception.cpp
//Author: Martin Krapcho
//E-Mail: KrapchoM@imsweb.com
//NCI Contacts: Kevin Dodd, Barnali Das
//------------------------------------------------------------------------------
//Version 1.0.0
//------------------------------------------------------------------------------

#include "sim_exception.h"
#include <iostream>
#include <cstring>

//------------------------------------------------------------------------------
SimException::SimException(const char* sErrorCallPath, const char* sThrownError, eExceptType eType)
{
   strcpy(gsError,sThrownError);
   strcpy(gsCallPath,sErrorCallPath);
   if(eType >= 0 &&  eType < NUM_VALUES)
      geType = eType;
   else
      eType  = FATAL;
}

//------------------------------------------------------------------------------
void  SimException::AddCallPath(const char* sAddCallPath)
{
   strcat(gsCallPath,"|");
   strcat(gsCallPath,sAddCallPath);
}

