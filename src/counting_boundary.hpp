#ifndef COUNTING_BOUNDARY_HPP
#define COUNTING_BOUNDARY_HPP

#include "utils.hpp"
#include "cuddObj.hh"
#include <set>

/**
 * Boundary Counting Approach
 *
 * This is the original counting method that counts pairs of regions (M1x, M2x) as one solution.
 * For subproblems with bitmasks M1 and M2, if some assignment x satisfies the threshold in the BDD,
 * it counts the pair (M1x, M2x) as one solution using BDD threshold and minterm counting.
 */

namespace CountingBoundary
{
    /**
     * Performs boundary counting on the difference ADD
     * @param add The ADD representing the difference between two ensembles
     * @param manager The CUDD manager
     * @param gap The threshold value
     * @return Number of satisfying assignments (boundary count)
     */
    long long countSolutions(BDD &bdd, Cudd &manager, double gap, const std::vector<bool> &bitmask1, const std::vector<bool> &bitmask2, bool dump_assignments, const std::string &dump_file);

}

#endif // COUNTING_BOUNDARY_HPP