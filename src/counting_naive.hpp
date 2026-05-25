#ifndef COUNTING_NAIVE_HPP
#define COUNTING_NAIVE_HPP

#include "utils.hpp"
#include "cuddObj.hh"
#include <set>
#include <vector>
#include <unordered_set>
#include <string>

/**
 * Naive Counting Approach
 *
 * This counting method stores individual M1x and M2x values in separate global hashmaps.
 * For each satisfying assignment x, it computes M1x and M2x using the bitmasks
 * and stores each unique value separately. The final count is the union of both sets.
 */

namespace CountingNaive
{

    // Global hashset for naive counting - stores unique M1x and M2x values
    extern std::unordered_set<std::string> globalSolutionSet;

    /**
     * Clear the global solution set (call this at the start of the program)
     */
    void clearGlobalSet();

    /**
     * Get the current size of the global solution set
     * @return Size of the global solution set
     */
    size_t getGlobalSetSize();

    /**
     * Get all elements from the global solution set
     * @return Vector containing all elements in the global set
     */
    std::vector<std::string> getGlobalSetElements();

    /**
     * Helper function to enumerate all satisfying assignments of a BDD
     * @param bdd The BDD to enumerate
     * @param manager The CUDD manager
     * @param bitmask1 Bitmask for M1x
     * @param bitmask2 Bitmask for M2x
     * @param newSolutionsCount Reference to count new solutions added
     */
    void enumerateSatisfyingAssignments(BDD bdd, Cudd &manager,
                                        const std::vector<bool> &bitmask1,
                                        const std::vector<bool> &bitmask2,
                                        long long &newSolutionsCount);

    /**
     * Performs naive counting by enumerating satisfying assignments
     * @param add The ADD representing the difference between two ensembles
     * @param manager The CUDD manager
     * @param gap The threshold value
     * @param bitmask1 Bitmask for the first ensemble (M1)
     * @param bitmask2 Bitmask for the second ensemble (M2)
     * @return Number of new unique M1x and M2x values added to global sets
     */
    long long countSolutions(BDD &bdd, Cudd &manager, double gap,
                             const std::vector<bool> &bitmask1, const std::vector<bool> &bitmask2, bool dump_assignments = false, const std::string& dump_file = "");

}

#endif // COUNTING_NAIVE_HPP