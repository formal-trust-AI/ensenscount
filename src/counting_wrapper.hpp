#ifndef COUNTING_WRAPPER_HPP
#define COUNTING_WRAPPER_HPP

#include "utils.hpp"
#include "cuddObj.hh"
#include <set>
#include <vector>
#include <string>

/**
 * Central Counting Wrapper
 *
 * This file provides a unified interface for all counting methods.
 * The countSol function acts as a dispatcher that delegates to the appropriate
 * counting method based on the global configuration.
 */

namespace CountingWrapper
{

    /**
     * Simplified counting function that uses global configuration
     * @param add The ADD representing the difference between two ensembles
     * @param manager The CUDD manager
     * @param gap The threshold value
     * @return Number of solutions according to the configured counting method
     */
    long long countSol(BDD &bdd, Cudd &manager, double gap);

    /**
     * Unified counting function that delegates to the appropriate counting method
     * @param add The ADD representing the difference between two ensembles
     * @param manager The CUDD manager
     * @param gap The threshold value
     * @param countingMethod The counting method to use ("boundary", "naive", etc.)
     * @param bitmask1 Bitmask for the first ensemble (used by naive counting)
     * @param bitmask2 Bitmask for the second ensemble (used by naive counting)
     * @return Number of solutions according to the specified counting method
     */
    long long countSol(BDD &bdd, Cudd &manager, double gap,
                       const std::string &countingMethod,
                       const std::vector<bool> &bitmask1,
                       const std::vector<bool> &bitmask2
                       );

    /**
     * Get the final count for naive counting (size of global set)
     * For boundary counting, this just returns the sum of all subproblem counts
     * @param subproblemSum Sum of all subproblem counts
     * @param countingMethod The counting method that was used
     * @return Final count according to the counting method
     */
    long long getFinalCount(long long subproblemSum, const std::string &countingMethod);

    /**
     * Display global set elements (for naive counting method)
     */
    void displayGlobalSetElements();
}

#endif // COUNTING_WRAPPER_HPP