//Class Author: Martin Krapcho, Information Manangement Services, Inc.
//e-mail : KrapchoM@imsweb.com
//Mersenne Twister Code written by: Makoto Matsumoto and Takuji Nishimura

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


#include "mersenne_class.h"

//==============================================================================
//Mersenne Twister Random Number Functions
//==============================================================================

MersenneTwister::MersenneTwister(unsigned long ulSeed){
   mti=N_SIZE+1; /* mti==N_SIZE+1 means mt[N_SIZE] is not initialized */

   gulSeed = ulSeed;

   init_genrand(gulSeed);
}

MersenneTwister::~MersenneTwister(){
   ;
}

/* initializes mt[N_SIZE] with a seed */
void MersenneTwister::init_genrand(unsigned long s)
{
    mt[0]= s & 0xffffffffUL;
    for (mti=1; mti<N_SIZE; mti++) {
        mt[mti] =
	    (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        mt[mti] &= 0xffffffffUL;
        /* for >32 bit machines */
    }
}

/* generates a random number on [0,0xffffffff]-interval */
unsigned long MersenneTwister::genrand_int32(void)
{
    unsigned long y;
    static unsigned long mag01[2]={0x0UL, MATRIX_A};
    /* mag01[x] = x * MATRIX_A  for x=0,1 */

    if (mti >= N_SIZE) { /* generate N_SIZE words at one time */
        int kk;

        if (mti == N_SIZE+1)   /* if init_genrand() has not been called, */
            init_genrand(5489UL); /* a default initial seed is used */

        for (kk=0;kk<N_SIZE-M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N_SIZE-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M-N_SIZE)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N_SIZE-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N_SIZE-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }

    y = mt[mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

/* generates a random number on [0,0x7fffffff]-interval */
long MersenneTwister::genrand_int31(void)
{
    return (long)(genrand_int32()>>1);
}

/* generates a random number on [0,1]-real-interval */
double MersenneTwister::genrand_real1(void)
{
    return genrand_int32()*(1.0/4294967295.0);
    /* divided by 2^32-1 */
}

/* generates a random number on [0,1)-real-interval */
double MersenneTwister::genrand_real2(void)
{
    return genrand_int32()*(1.0/4294967296.0);
    /* divided by 2^32 */
}


