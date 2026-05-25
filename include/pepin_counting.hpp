#ifndef PEPIN_COUNTING_INTERFACE_HPP
#define PEPIN_COUNTING_INTERFACE_HPP

#include <vector>
#include "cuddObj.hh"

using namespace std;

namespace PepinCounting
{
    // Initialize global Pepin parameters
    void initGlobalPepin(double eps, double delta, unsigned int seed, bool enableDebug = false, bool enableSanityCheck = false);

    // Set the total number of subproblems (m value)
    void setTotalSubproblems(int m);

    // Main counting interface that matches the expected signature
    long long countSolutions(BDD &bdd, Cudd &manager, double gap,
                             const vector<bool> &bitmask1,
                             const vector<bool> &bitmask2,
                             double eps, double delta, unsigned int seed);

    // Get final scaled count
    long long getFinalGlobalCount();

    // Clear global state
    void clearGlobalPepin();

} // namespace PepinCounting

#endif // PEPIN_COUNTING_INTERFACE_HPP