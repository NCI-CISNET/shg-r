#include <iostream>
#include <random>
#include <Rcpp.h>

// [[Rcpp::export]]
void gen5num() {
  std::mt19937 mt; // Mersenne Twister engine
  /* Initial scrambling */
  int seed = 12345;

  mt.seed(seed);   // Set the seed
//https://github.com/wch/r-source/blob/4a5e54e99fa50a9daa12e8d35b9b6b1ab40bf63a/src/main/RNG.c#L625
  std::uniform_real_distribution<double> dist(0.0, 1.0); // Distribution for random numbers

  // Generate and print 5 random numbers
  for (int i = 0; i < 5; ++i) {
    double random_number = dist(mt);
    std::cout << random_number << std::endl;
  }

//return 0;
}

// RNG.c in R source code on Github
// https://github.com/wch/r-source/blob/4a5e54e99fa50a9daa12e8d35b9b6b1ab40bf63a/src/main/RNG.c#L571
