//Class Author: Martin Krapcho, Information Manangement Services, Inc.
//e-mail : KrapchoM@imsweb.com
//Mersenne Twister Code written by: Makoto Matsumoto and Takuji Nishimura

#ifndef _MERSENNE_H
#define _MERSENNE_H

//Note: This class was made based on code obtained for the Mersenne Twister PRNG
// The program code was converted into a class to allow multiple PRNGs in the same program.
// Copyright information for the Mersenne Twister code that is used in this class
// is as follows.
//---------------------------------------------------------------------------
//Mersenne Twister Pseudo Random Number Generator
//---------------------------------------------------------------------------
//Mersenne Twister Code obtained from
//http://www.math.keio.ac.jp/matumoto/CODES/MT2002/mt19937ar.c
//Copyright Notice:
//   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
//   All rights reserved.
//Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     1. Redistributions of source code must retain the above copyright
//        notice, this list of conditions and the following disclaimer.
//
//     2. Redistributions in binary form must reproduce the above copyright
//        notice, this list of conditions and the following disclaimer in the
//        documentation and/or other materials provided with the distribution.
//
//     3. The names of its contributors may not be used to endorse or promote
//        products derived from this software without specific prior written
//        permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.



/* Period parameters */
#define N_SIZE 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

class MersenneTwister{
   private:
      unsigned long gulSeed; //Seed used to initialize generator
      unsigned long mt[N_SIZE]; /* the array for the state vector  */
      int mti; /* mti==N_SIZE+1 means mt[N_SIZE] is not initialized */

      void           init_genrand(unsigned long s);

   public:
      MersenneTwister(unsigned long ulSeed);      //Constructor
      ~MersenneTwister();     //Destructor


      unsigned long  genrand_int32(void);
      long           genrand_int31(void);
      double         genrand_real1(void);
      double         genrand_real2(void);

      unsigned long  GetSeed()            {return gulSeed;};

};


#endif
