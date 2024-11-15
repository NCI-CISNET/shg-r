// CISNET (www.cisnet.cancer.gov)
// Lung Cancer Base Case Group
// Smoking History Simulation Application
// Application to Simulate Initiation and Cessation Ages of individuals based on sex, race and year of birth.
// File: rng_strategy.h
// Author: John Clarke (Cornerstone Systems Northwest)
// E-Mail: john.clarke@cornerstonenw.com
// This helper class provides a strategy pattern for the RNGs used in the SHG simulation.
// You can add your own strategies with other RNG engines by implementing the RNG_Strategy interface.
// Two classes are provided below: MersenneTwisterRNG and RngStreamRNG.
// MerseeneTwisterRNG uses the Mersenne Twister RNG engine and provides legacy support for the original SHG simulation.
// RngStreamRNG uses the RngStream RNG engine written by Pierre L'Ecuyer (University of Montreal) lecuyer@iro.umontreal.ca
// RngStreamRNG utilizes substreams which guarantees IID properties and allows for IID parallel processing

#ifndef RNG_STRATEGY_H
#define RNG_STRATEGY_H

#ifdef IS_RCPP
#include <Rcpp.h>
#endif

// RNG Strategy Interface
class RNG_Strategy {
public:
    virtual ~RNG_Strategy() {}
    virtual void initialize() = 0;
    // Overloaded method for MersenneTwister
    void initialize(unsigned long ulInitSeed, unsigned long ulCessSeed, unsigned long ulLifeTableSeed, unsigned long ulIndRndsSeed);
    // Overloaded method for RngStream
    void initialize(unsigned long seed[6]); 
    virtual double getInitiationRand() = 0;
    virtual double getCessationRand() = 0;
    virtual double getLifeTableRand() = 0;
    virtual double getIndividualRand() = 0;
    virtual void resetStrategy() = 0;  // resets all RNGs to their initial state
    virtual void writeRNGState() = 0;  // writes the current state of the RNGs to the console
    virtual void incrementSubstreams() = 0;
    void incrementSubstreams(int n) {
        // Helper method to increment the 4 substream sets n times
        for (int i = 0; i < n; i++) {
          incrementSubstreams();
        }
    }
    void resetCounters() {
        lInitiationRandCount = 0;
        lCessationRandCount = 0;
        lLifeTableRandCount = 0;
        lIndividualRandCount = 0;
    }

    long lInitiationRandCount = 0;
    long lCessationRandCount = 0;
    long lLifeTableRandCount = 0;
    long lIndividualRandCount = 0;
};

// MersenneTwisterRNG Strategy
#include "mersenne_class.h"

class MersenneTwisterRNG : public RNG_Strategy {
public:
    MersenneTwisterRNG() {
        initialize();
    }
    MersenneTwisterRNG(unsigned long ulInitSeed, unsigned long ulCessSeed, unsigned long ulLifeTableSeed, unsigned long ulIndRndsSeed) {
        initialize(ulInitSeed, ulCessSeed, ulLifeTableSeed, ulIndRndsSeed);
    }
    // Free the dynamically allocated memory
    ~MersenneTwisterRNG()
    {
        // TODO: ensure that we aren't missing anything here
        delete gpInitiationRNG;
        delete gpCessationRNG; 
        delete gpLifeTableRNG;
        delete gpIndividualRNG;
    }

    void initialize(unsigned long ulInitSeed, unsigned long ulCessSeed, unsigned long ulLifeTableSeed, unsigned long ulIndRndsSeed) {
        gpInitiationRNG = new MersenneTwister(ulInitSeed);
        gpCessationRNG  = new MersenneTwister(ulCessSeed);
        gpLifeTableRNG  = new MersenneTwister(ulLifeTableSeed);
        gpIndividualRNG = new MersenneTwister(ulIndRndsSeed);

    }
    void initialize() override {
        // default MT seeds also used in run_tests.py
        unsigned long ulInitSeed      = 1898587603;
        unsigned long ulCessSeed      = 1468371936;
        unsigned long ulLifeTableSeed = 1551308340;
        unsigned long ulIndRndsSeed   = 1590227640;
        initialize(ulInitSeed, ulCessSeed, ulLifeTableSeed, ulIndRndsSeed);
    }
    double getInitiationRand() override {
        lInitiationRandCount++;
        return gpInitiationRNG->genrand_real1();
    }
    double getCessationRand() override {
        lCessationRandCount++;
        return gpCessationRNG->genrand_real1();
    }
    double getLifeTableRand() override {
        lLifeTableRandCount++;
        return gpLifeTableRNG->genrand_real1();
    }
    double getIndividualRand() override {
        lIndividualRandCount++;
        return gpIndividualRNG->genrand_real1();
    }
    void resetStrategy() override {
        // reset all RNGs to their initial state
        initialize(ulInitiationSeed, ulCessationSeed, ulLifeTableSeed, ulIndividualSeed);
    }
    void incrementSubstreams() override {
        // Set all RNGs to the next unused substream (without colliding with existing substreams)
        // Because we have 4 RNGs, we need to increment 4 times to avoid collisions: (1,2,3,4 -> 5,6,7,8)
        // TODO implement something similar for MersenneTwister even though it doesn't have the substream feature
    }
    void writeRNGState() override {
        // TODO return MT state
    }

private:
    MersenneTwister *gpInitiationRNG;
    MersenneTwister *gpCessationRNG;
    MersenneTwister *gpLifeTableRNG;
    MersenneTwister *gpIndividualRNG;

    unsigned long ulInitiationSeed;
    unsigned long ulCessationSeed;
    unsigned long ulLifeTableSeed;
    unsigned long ulIndividualSeed;
};

// RngStreamRNG Strategy
#include "RngStream.h"

class RngStreamRNG : public RNG_Strategy {
public:
    RngStreamRNG() {
        initialize();
    }
    RngStreamRNG(unsigned long seed[6]) {
        initialize(seed);
    }
    // Free the dynamically allocated memory
    ~RngStreamRNG()
    {
        // TODO: ensure that we aren't missing anything here
        delete gpInitiationRNG;
        delete gpCessationRNG; 
        delete gpLifeTableRNG;
        delete gpIndividualRNG;
    }
    void initialize() override {
        // use default seed defined in RngStream.cpp (12345, 12345, 12345, 12345, 12345, 12345)
        // once the initial seed is set for the Initiation stream, we use substreams for the other RNGs
        gpInitiationRNG = new RngStream();
        const unsigned long seed[6] = {12345, 12345, 12345, 12345, 12345, 12345};
        gpInitiationRNG->SetPackageSeed(seed);

        gpCessationRNG = new RngStream(*gpInitiationRNG);
        gpCessationRNG->ResetNextSubstream();
        gpLifeTableRNG  = new RngStream(*gpCessationRNG);
        gpLifeTableRNG->ResetNextSubstream();
        gpIndividualRNG = new RngStream(*gpLifeTableRNG);
        gpIndividualRNG->ResetNextSubstream();
    }

    void initialize(unsigned long seed[6]) {
        // Offering the possibility to set a seed but typically not needed
        // Once the initial seed is set for the Initiation stream, we use substreams for the other RNGs
        gpInitiationRNG = new RngStream();
        gpInitiationRNG->SetSeed(seed);
        gpCessationRNG = new RngStream(*gpInitiationRNG);
        gpCessationRNG->ResetNextSubstream();
        gpLifeTableRNG  = new RngStream(*gpCessationRNG);
        gpLifeTableRNG->ResetNextSubstream();
        gpIndividualRNG = new RngStream(*gpLifeTableRNG);
        gpIndividualRNG->ResetNextSubstream();
    }
    double getInitiationRand() override {
        lInitiationRandCount++;
        return gpInitiationRNG->RandU01();
    }
    double getCessationRand() override {
        lCessationRandCount++;
        return gpCessationRNG->RandU01();
    }
    double getLifeTableRand() override {
        lLifeTableRandCount++;
        return gpLifeTableRNG->RandU01();
    }
    double getIndividualRand() override {
        lIndividualRandCount++;
        return gpIndividualRNG->RandU01();
    }
    void resetStrategy() override {
        // Reset all RNGs to their initial states
        gpInitiationRNG->ResetStartSubstream(); // same as ResetSubstream() for gpInitiationRNG
        gpCessationRNG->ResetStartSubstream();
        gpLifeTableRNG->ResetStartSubstream();
        gpIndividualRNG->ResetStartSubstream();
    }
    void incrementSubstreams() override {
        // Set all RNGs to the next unused substream (without colliding with existing substreams)
        // Because we have 4 RNGs, we need to increment 4 times to avoid collisions: (1,2,3,4 -> 5,6,7,8)

        for (int i = 0; i < 4; i++) {
            gpInitiationRNG->ResetNextSubstream();
            gpCessationRNG->ResetNextSubstream();
            gpLifeTableRNG->ResetNextSubstream();
            gpIndividualRNG->ResetNextSubstream();
        }
    }
    void writeRNGState() override {
        // Ig[6] Initial state of stream (master seed)
        // Bg[6] State of current substream
        // Cg[6] Current state (in subset of current substream)
        // See https://www-labs.iro.umontreal.ca/~lecuyer/myftp/papers/streams00.pdf

        #ifdef IS_RCPP
            Rcpp::Rcout << "Initiation RNG State:" << std::endl;
            gpInitiationRNG->WriteStateFull();
            Rcpp::Rcout << "Cessation RNG State:" << std::endl;
            gpCessationRNG->WriteStateFull();
            Rcpp::Rcout << "Life Table RNG State:" << std::endl;
            gpLifeTableRNG->WriteStateFull();
            Rcpp::Rcout << "Individual RNG State:" << std::endl;
            gpIndividualRNG->WriteStateFull();
            Rcpp::Rcout << "----- Done -----" << std::endl;
        #else
            std::cout << "writeRNGState() not yet implemented for CLI" << std::endl;
        #endif
    }

    // TODO Add garbage collection

private:
    //RngStream rngStream; // Instance of your RngStream class
    RngStream *gpInitiationRNG;
    RngStream *gpCessationRNG;
    RngStream *gpLifeTableRNG;
    RngStream *gpIndividualRNG;
};

#endif // RNG_STRATEGY_H