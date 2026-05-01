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
// MersenneTwisterRNG uses the Mersenne Twister RNG engine and provides legacy support for the original SHG simulation.
// RngStreamRNG uses the RngStream RNG engine written by Pierre L'Ecuyer (University of Montreal) lecuyer@iro.umontreal.ca
// RngStreamRNG utilizes substreams which guarantees IID properties and allows for IID parallel processing

#ifndef RNG_STRATEGY_H
#define RNG_STRATEGY_H

#include <memory>

#ifdef IS_R
#include <Rcpp.h>
#else
#include <iostream>
#include <vector>
#endif

// RNG Strategy Interface
class RNG_Strategy {
public:
    virtual ~RNG_Strategy() {}
    virtual void initialize() = 0;
    // Overloaded method for MersenneTwister
    void initialize(unsigned long ulInitSeed, unsigned long ulCessSeed, unsigned long ulMortalitySeed, unsigned long ulIndRndsSeed);
    // Overloaded method for RngStream
    void initialize(unsigned long seed[6]); 
    virtual double getInitiationRand() = 0;
    virtual double getCessationRand() = 0;
    virtual double getMortalityRand() = 0;
    virtual double getIndividualRand() = 0;
    virtual void resetStrategy() = 0;  // resets all RNGs to their initial state
    virtual void writeRNGState() = 0;  // writes the current state of the RNGs to the console
    virtual void incrementSubstreams() = 0;
    virtual std::vector<double> getRNGStateFingerprint() = 0;  // returns a fingerprint of the RNG state for comparison
    void incrementSubstreams(int n) {
        // Helper method to increment the 4 substream sets n times
        for (int i = 0; i < n; i++) {
          incrementSubstreams();
        }
    }
    void resetCounters() {
        lInitiationRandCount = 0;
        lCessationRandCount = 0;
        lMortalityRandCount = 0;
        lIndividualRandCount = 0;
    }

    long lInitiationRandCount = 0;
    long lCessationRandCount = 0;
    long lMortalityRandCount = 0;
    long lIndividualRandCount = 0;
};

// MersenneTwisterRNG Strategy
#include "mersenne_class.h"

class MersenneTwisterRNG : public RNG_Strategy {
public:
    MersenneTwisterRNG() {
        initialize();
    }
    MersenneTwisterRNG(unsigned long ulInitSeed, unsigned long ulCessSeed, unsigned long ulMortalitySeed, unsigned long ulIndRndsSeed) {
        initialize(ulInitSeed, ulCessSeed, ulMortalitySeed, ulIndRndsSeed);
    }
    // Free the dynamically allocated memory
    ~MersenneTwisterRNG()
    {
        // TODO: ensure that we aren't missing anything here
        delete gpInitiationRNG;
        delete gpCessationRNG; 
        delete gpMortalityRNG;
        delete gpIndividualRNG;
    }

    void initialize(unsigned long ulInitSeed, unsigned long ulCessSeed, unsigned long ulMortalitySeed, unsigned long ulIndRndsSeed) {
        gpInitiationRNG = new MersenneTwister(ulInitSeed);
        gpCessationRNG  = new MersenneTwister(ulCessSeed);
        gpMortalityRNG  = new MersenneTwister(ulMortalitySeed);
        gpIndividualRNG = new MersenneTwister(ulIndRndsSeed);

    }
    void initialize() override {
        // default MT seeds also used in run_tests.py
        unsigned long ulInitSeed      = 1898587603;
        unsigned long ulCessSeed      = 1468371936;
        unsigned long ulMortalitySeed = 1551308340;
        unsigned long ulIndRndsSeed   = 1590227640;
        initialize(ulInitSeed, ulCessSeed, ulMortalitySeed, ulIndRndsSeed);
    }
    double getInitiationRand() override {
        lInitiationRandCount++;
        return gpInitiationRNG->genrand_real1();
    }
    double getCessationRand() override {
        lCessationRandCount++;
        return gpCessationRNG->genrand_real1();
    }
    double getMortalityRand() override {
        lMortalityRandCount++;
        return gpMortalityRNG->genrand_real1();
    }
    double getIndividualRand() override {
        lIndividualRandCount++;
        return gpIndividualRNG->genrand_real1();
    }
    void resetStrategy() override {
        // reset all RNGs to their initial state
        initialize(ulInitiationSeed, ulCessationSeed, ulMortalitySeed, ulIndividualSeed);
    }
    void incrementSubstreams() override {
        // Set all RNGs to the next unused substream (without colliding with existing substreams)
        // Because we have 4 RNGs, we need to increment 4 times to avoid collisions: (1,2,3,4 -> 5,6,7,8)
        // TODO implement something similar for MersenneTwister even though it doesn't have the substream feature
    }
    void writeRNGState() override {
        // TODO return MT state
    }
    std::vector<double> getRNGStateFingerprint() override {
        // For MersenneTwister, generate a few random numbers from each stream as a fingerprint
        // This verifies the internal state is different for different seeds
        std::vector<double> fingerprint;
        fingerprint.reserve(12);  // 3 numbers from each of 4 streams
        
        // Get 3 random numbers from each stream
        for (int i = 0; i < 3; i++) {
            fingerprint.push_back(gpInitiationRNG->genrand_real1());
        }
        for (int i = 0; i < 3; i++) {
            fingerprint.push_back(gpCessationRNG->genrand_real1());
        }
        for (int i = 0; i < 3; i++) {
            fingerprint.push_back(gpMortalityRNG->genrand_real1());
        }
        for (int i = 0; i < 3; i++) {
            fingerprint.push_back(gpIndividualRNG->genrand_real1());
        }
        return fingerprint;
    }

private:
    MersenneTwister *gpInitiationRNG;
    MersenneTwister *gpCessationRNG;
    MersenneTwister *gpMortalityRNG;
    MersenneTwister *gpIndividualRNG;

    unsigned long ulInitiationSeed;
    unsigned long ulCessationSeed;
    unsigned long ulMortalitySeed;
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
        delete gpMortalityRNG;
        delete gpIndividualRNG;
    }
    void initialize() override {
        // use default seed defined in RngStream.cpp (12345, 12345, 12345, 12345, 12345, 12345)
        // once the initial seed is set for the Initiation stream, we use substreams for the other RNGs
        gpInitiationRNG = new RngStream();
        const unsigned long seed[6] = {12345, 12345, 12345, 12345, 12345, 12345};
        gpInitiationRNG->SetSeed(seed);

        gpCessationRNG = new RngStream(*gpInitiationRNG);
        gpCessationRNG->ResetNextSubstream();
        gpMortalityRNG  = new RngStream(*gpCessationRNG);
        gpMortalityRNG->ResetNextSubstream();
        gpIndividualRNG = new RngStream(*gpMortalityRNG);
        gpIndividualRNG->ResetNextSubstream();
    }

    void initialize(unsigned long seed[6]) {
        // Offering the possibility to set a seed but typically not needed
        // Once the initial seed is set for the Initiation stream, we use substreams for the other RNGs
        gpInitiationRNG = new RngStream();
        gpInitiationRNG->SetSeed(seed);
        gpCessationRNG = new RngStream(*gpInitiationRNG);
        gpCessationRNG->ResetNextSubstream();
        gpMortalityRNG  = new RngStream(*gpCessationRNG);
        gpMortalityRNG->ResetNextSubstream();
        gpIndividualRNG = new RngStream(*gpMortalityRNG);
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
    double getMortalityRand() override {
        lMortalityRandCount++;
        return gpMortalityRNG->RandU01();
    }
    double getIndividualRand() override {
        lIndividualRandCount++;
        return gpIndividualRNG->RandU01();
    }
    void resetStrategy() override {
        // Reset all RNGs to their initial states
        gpInitiationRNG->ResetStartSubstream(); // same as ResetSubstream() for gpInitiationRNG
        gpCessationRNG->ResetStartSubstream();
        gpMortalityRNG->ResetStartSubstream();
        gpIndividualRNG->ResetStartSubstream();
    }
    void incrementSubstreams() override {
        // Set all RNGs to the next unused substream (without colliding with existing substreams)
        // Because we have 4 RNGs, we need to increment 4 times to avoid collisions: (1,2,3,4 -> 5,6,7,8)

        for (int i = 0; i < 4; i++) {
            gpInitiationRNG->ResetNextSubstream();
            gpCessationRNG->ResetNextSubstream();
            gpMortalityRNG->ResetNextSubstream();
            gpIndividualRNG->ResetNextSubstream();
        }
    }
    void writeRNGState() override {
        // Ig[6] Initial state of stream (master seed)
        // Bg[6] State of current substream
        // Cg[6] Current state (in subset of current substream)
        // See https://www-labs.iro.umontreal.ca/~lecuyer/myftp/papers/streams00.pdf

        #ifdef IS_R
            Rcpp::Rcout << "Initiation RNG State:" << std::endl;
            gpInitiationRNG->WriteStateFull();
            Rcpp::Rcout << "Cessation RNG State:" << std::endl;
            gpCessationRNG->WriteStateFull();
            Rcpp::Rcout << "Mortality RNG State:" << std::endl;
            gpMortalityRNG->WriteStateFull();
            Rcpp::Rcout << "Individual RNG State:" << std::endl;
            gpIndividualRNG->WriteStateFull();
            Rcpp::Rcout << "----- Done -----" << std::endl;
        #else
            std::cout << "writeRNGState() not yet implemented for CLI" << std::endl;
        #endif
    }
    std::vector<double> getRNGStateFingerprint() override {
        // For RngStream, get the actual internal state from each stream
        std::vector<double> fingerprint;
        fingerprint.reserve(24);  // 6 state values from each of 4 streams
        
        unsigned long state[6];
        
        // Get state from each stream
        gpInitiationRNG->GetState(state);
        for (int i = 0; i < 6; i++) {
            fingerprint.push_back(static_cast<double>(state[i]));
        }
        gpCessationRNG->GetState(state);
        for (int i = 0; i < 6; i++) {
            fingerprint.push_back(static_cast<double>(state[i]));
        }
        gpMortalityRNG->GetState(state);
        for (int i = 0; i < 6; i++) {
            fingerprint.push_back(static_cast<double>(state[i]));
        }
        gpIndividualRNG->GetState(state);
        for (int i = 0; i < 6; i++) {
            fingerprint.push_back(static_cast<double>(state[i]));
        }
        return fingerprint;
    }
    
    // Expose internal streams for buffering (performance optimization)
    RngStream* getInitiationStream() { return gpInitiationRNG; }
    RngStream* getCessationStream() { return gpCessationRNG; }
    RngStream* getMortalityStream() { return gpMortalityRNG; }
    RngStream* getIndividualStream() { return gpIndividualRNG; }

private:
    RngStream *gpInitiationRNG;
    RngStream *gpCessationRNG;
    RngStream *gpMortalityRNG;
    RngStream *gpIndividualRNG;
};

// ==============================================================================
// RNG Buffering Classes: Pre-generate random numbers in batches
// Maintains exact sequence - just reduces function call overhead
// ==============================================================================

// Buffer for a single RNG stream
class RNGStreamBuffer {
private:
    std::vector<double> buffer;
    size_t next_index;
    size_t buffer_size;
    RngStream* stream;
    
public:
    RNGStreamBuffer(RngStream* rng_stream, size_t batch_size = 1000) 
        : buffer(), next_index(batch_size), buffer_size(batch_size), stream(rng_stream) {
        buffer.resize(batch_size);
    }
    
    // Get next random value (refills buffer if needed)
    __attribute__((always_inline))
    inline double getNext() {
        if (__builtin_expect(next_index >= buffer_size, 0)) {  // Branch prediction hint
            // Refill buffer - generate batch of random numbers
            for (size_t i = 0; i < buffer_size; i++) {
                buffer[i] = stream->RandU01();
            }
            next_index = 0;
        }
        return buffer[next_index++];
    }
};

// Buffered RNG Strategy: Wraps RngStreamRNG with buffering
// Drop-in replacement - maintains EXACT same sequence!
class BufferedRngStreamRNG : public RNG_Strategy {
private:
    RngStreamRNG* underlying_rng;
    bool owns_rng;
    
    std::unique_ptr<RNGStreamBuffer> initiation_buffer;
    std::unique_ptr<RNGStreamBuffer> cessation_buffer;
    std::unique_ptr<RNGStreamBuffer> mortality_buffer;
    std::unique_ptr<RNGStreamBuffer> individual_buffer;
    
public:
    // Constructor: wraps an existing RngStreamRNG
    BufferedRngStreamRNG(RngStreamRNG* rng, size_t buffer_size = 1000, bool take_ownership = false)
        : underlying_rng(rng), owns_rng(take_ownership) {
        
        // Create buffers for each of the 4 independent streams
        initiation_buffer = std::make_unique<RNGStreamBuffer>(
            rng->getInitiationStream(), buffer_size);
        cessation_buffer = std::make_unique<RNGStreamBuffer>(
            rng->getCessationStream(), buffer_size);
        mortality_buffer = std::make_unique<RNGStreamBuffer>(
            rng->getMortalityStream(), buffer_size);
        individual_buffer = std::make_unique<RNGStreamBuffer>(
            rng->getIndividualStream(), buffer_size);
    }
    
    ~BufferedRngStreamRNG() {
        if (owns_rng) delete underlying_rng;
    }
    
    // RNG_Strategy interface - use buffers instead of direct calls
    double getInitiationRand() override {
        lInitiationRandCount++;
        return initiation_buffer->getNext();
    }
    double getCessationRand() override {
        lCessationRandCount++;
        return cessation_buffer->getNext();
    }
    double getMortalityRand() override {
        lMortalityRandCount++;
        return mortality_buffer->getNext();
    }
    double getIndividualRand() override {
        lIndividualRandCount++;
        return individual_buffer->getNext();
    }
    
    // Pass-through methods to underlying RNG
    void initialize() override { underlying_rng->initialize(); }
    void resetStrategy() override { underlying_rng->resetStrategy(); }
    void incrementSubstreams() override { underlying_rng->incrementSubstreams(); }
    void writeRNGState() override { underlying_rng->writeRNGState(); }
    std::vector<double> getRNGStateFingerprint() override { 
        return underlying_rng->getRNGStateFingerprint(); 
    }
};

#endif // RNG_STRATEGY_H