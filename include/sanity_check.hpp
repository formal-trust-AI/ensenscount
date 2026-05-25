#ifndef SANITY_CHECK_HPP
#define SANITY_CHECK_HPP

#include <vector>
#include <string>
#include "cuddObj.hh"

using namespace std;

namespace SanityCheck
{
    /**
     * Enable or disable sanity checking globally
     */
    void enableSanityCheck(bool enable);

    /**
     * Enumerate all assignments from a BDD and perform sanity checks
     * @param bdd The BDD to enumerate assignments from
     * @param subproblem_id Identifier for the subproblem (for logging)
     * @param num_features Number of features in the dataset
     * @param features_per_bit Vector indicating how many bits each feature uses
     * @return Number of valid assignments found
     */
    long long enumerateAndCheckAssignments(const BDD &bdd, int subproblem_id,
                                           int num_features, const vector<int> &features_per_bit);

    /**
     * Check if a single assignment is valid according to feature ordering constraints
     * @param assignment The assignment bitstring
     * @param num_features Number of features
     * @param features_per_bit Vector indicating how many bits each feature uses
     * @return true if assignment is valid, false otherwise
     */
    bool isAssignmentValid(const vector<int> &assignment, int num_features,
                           const vector<int> &features_per_bit);

    /**
     * Split assignment bitstring by features and check ordering constraints
     * @param assignment The assignment bitstring
     * @param num_features Number of features
     * @param features_per_bit Vector indicating how many bits each feature uses
     * @param subproblem_id For logging purposes
     * @return true if all features satisfy ordering constraints
     */
    bool checkFeatureOrdering(const vector<int> &assignment, int num_features,
                              const vector<int> &features_per_bit, int subproblem_id);

    /**
     * Validate cube constraints feature-wise: within each feature, 0 should not be followed by X, X should not be followed by 0
     * @param cube_pattern The cube pattern with 0, 1, and -1 (X) values
     * @param num_features Number of features
     * @param features_per_bit Vector indicating how many bits each feature uses
     * @param subproblem_id For logging purposes
     * @return true if cube satisfies constraints
     */
    bool validateCubeConstraints(const vector<int> &cube_pattern, int num_features, const vector<int> &features_per_bit, int subproblem_id);
}

#endif // SANITY_CHECK_HPP