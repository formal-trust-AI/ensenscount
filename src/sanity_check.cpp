#include "../include/sanity_check.hpp"
#include <iostream>
#include <set>

namespace
{
    bool sanity_check_enabled = false;
    std::set<std::vector<int>> all_assignments; // Store all found assignments
}

namespace SanityCheck
{
    void enableSanityCheck(bool enable)
    {
        sanity_check_enabled = enable;
        if (enable)
        {
            //std::cout << "Sanity checking enabled" << std::endl;
            all_assignments.clear();
        }
        else
        {
            //std::cout << "Sanity checking disabled" << std::endl;
        }
    }

    // Step 1: Enumerate all assignments from BDD
    long long enumerateAndCheckAssignments(const BDD &bdd, int subproblem_id,
                                           int num_features, const vector<int> &features_per_bit)
    {
        if (!sanity_check_enabled)
        {
            return 0; // Skip if sanity checking is disabled
        }

        std::cout << "\n=== SANITY CHECK for Subproblem " << subproblem_id << " ===" << std::endl;

        long long total_assignments = 0;
        long long valid_assignments = 0;

        std::cout << "Enumerating assignments from BDD..." << std::endl;

        // Get the manager and number of variables
        DdManager *manager = bdd.manager();
        if (manager == nullptr)
        {
            std::cout << "ERROR: BDD manager is null!" << std::endl;
            return 0;
        }

        int num_vars = Cudd_ReadSize(manager);
        std::cout << "BDD has " << num_vars << " variables" << std::endl;

        DdNode *bdd_node = bdd.getNode();
        if (bdd_node == nullptr)
        {
            std::cout << "ERROR: BDD node is null!" << std::endl;
            return 0;
        }

        // Use CUDD's cube enumeration to get all satisfying assignments
        DdGen *gen;
        int *cube;
        CUDD_VALUE_TYPE value;

        // Correct CUDD enumeration pattern - check if gen is not null
        long long total_individual_assignments = 0;
        int cubes_processed = 0;

        std::cout << "\nEnumerating cubes (each cube represents multiple assignments):" << std::endl;

        Cudd_ForeachCube(manager, bdd_node, gen, cube, value)
        {
            cubes_processed++;

            // Convert cube to assignment vector and count don't cares
            std::vector<int> cube_pattern(num_vars, -1);
            int dont_care_count = 0;
            for (int i = 0; i < num_vars; ++i)
            {
                if (cube[i] == 1)
                {
                    cube_pattern[i] = 1; // Variable is set to true
                }
                else if (cube[i] == 0)
                {
                    cube_pattern[i] = 0; // Variable is set to false
                }
                else
                {
                    cube_pattern[i] = -1; // Don't care
                    dont_care_count++;
                }
            }

            // Calculate how many individual assignments this cube represents
            long long assignments_in_cube = 1LL << dont_care_count; // 2^dont_care_count

            // Validate cube constraints feature-wise
            bool cube_valid = validateCubeConstraints(cube_pattern, num_features, features_per_bit, subproblem_id);

            if (cube_valid)
            {
                total_individual_assignments += assignments_in_cube;
                valid_assignments++;
            }
            else
            {
                // Skip invalid cubes
                std::cout << "  -> INVALID CUBE (constraint violation)" << std::endl;
            }

            // Print cube info (limit detailed output for readability)
            if (cubes_processed <= 10)
            {
                std::cout << "Cube " << cubes_processed << " (2^" << dont_care_count
                          << " = " << assignments_in_cube << " assignments): ";
                for (int val : cube_pattern)
                {
                    std::cout << (val == -1 ? 'X' : (val == 0 ? '0' : '1'));
                }
                if (cube_valid)
                {
                    std::cout << "  -> VALID CUBE" << std::endl;
                }
                else
                {
                    std::cout << "  -> INVALID CUBE" << std::endl;
                }
            }
            else if (cubes_processed % 1000 == 0)
            {
                std::cout << "Processed " << cubes_processed << " cubes, total assignments so far: "
                          << total_individual_assignments << std::endl;
            }

            // Store the cube pattern for later analysis
            all_assignments.insert(cube_pattern);
        }

        std::cout << "\n=== SUMMARY for Subproblem " << subproblem_id << " ===" << std::endl;
        std::cout << "Total cubes found: " << cubes_processed << std::endl;
        std::cout << "Valid cubes: " << valid_assignments << std::endl;
        std::cout << "Invalid cubes: " << (cubes_processed - valid_assignments) << std::endl;
        std::cout << "Total individual assignments from valid cubes: " << total_individual_assignments << std::endl;

        if (cubes_processed > 0)
        {
            double cube_validity_rate = (double)valid_assignments / cubes_processed * 100.0;
            std::cout << "Cube validity rate: " << cube_validity_rate << "%" << std::endl;
        }

        return total_individual_assignments;
    }

    // Helper function to validate cube constraints feature-wise
    bool validateCubeConstraints(const vector<int> &cube_pattern, int num_features, const vector<int> &features_per_bit, int subproblem_id)
    {
        bool all_valid = true;
        int bit_index = 0;

        for (int feature = 0; feature < num_features; ++feature)
        {
            int bits_for_feature = features_per_bit[feature];

            // Extract bits for this feature
            vector<int> feature_bits;
            for (int b = 0; b < bits_for_feature; ++b)
            {
                if (bit_index + b < cube_pattern.size())
                {
                    feature_bits.push_back(cube_pattern[bit_index + b]);
                }
            }

            // Check feature-wise constraints: within each feature
            // 0 should not be followed by X, X should not be followed by 0
            bool feature_valid = true;
            for (int b = 0; b < feature_bits.size() - 1; ++b)
            {
                // Check: 0 should not be followed by X
                if (feature_bits[b] == 0 && feature_bits[b + 1] == -1)
                {
                    if (subproblem_id >= 0)
                    {
                        std::cout << "CUBE VIOLATION in subproblem " << subproblem_id
                                  << ", feature " << feature
                                  << ", bit position " << b << "->" << (b + 1)
                                  << ": 0 followed by X" << std::endl;
                    }
                    feature_valid = false;
                    all_valid = false;
                }

                // Check: X should not be followed by 0
                if (feature_bits[b] == -1 && feature_bits[b + 1] == 0)
                {
                    if (subproblem_id >= 0)
                    {
                        std::cout << "CUBE VIOLATION in subproblem " << subproblem_id
                                  << ", feature " << feature
                                  << ", bit position " << b << "->" << (b + 1)
                                  << ": X followed by 0" << std::endl;
                    }
                    feature_valid = false;
                    all_valid = false;
                }
            }

            bit_index += bits_for_feature;
        }

        return all_valid;
    }

    // Step 2: Check if assignment satisfies feature ordering constraints
    bool isAssignmentValid(const vector<int> &assignment, int num_features,
                           const vector<int> &features_per_bit)
    {
        return checkFeatureOrdering(assignment, num_features, features_per_bit, -1);
    }

    // Step 3: Check feature-wise ordering constraints
    bool checkFeatureOrdering(const vector<int> &assignment, int num_features,
                              const vector<int> &features_per_bit, int subproblem_id)
    {
        if (!sanity_check_enabled)
        {
            return true; // Skip check if disabled
        }

        int bit_index = 0;
        bool all_valid = true;

        for (int feature = 0; feature < num_features; ++feature)
        {
            int bits_for_feature = features_per_bit[feature];

            // Extract bits for this feature
            vector<int> feature_bits;
            for (int b = 0; b < bits_for_feature; ++b)
            {
                if (bit_index + b < assignment.size())
                {
                    feature_bits.push_back(assignment[bit_index + b]);
                }
            }

            // Check ordering constraint: no 1 should be followed by 0
            // This represents: if not < threshold_i, then not < threshold_(i+1)
            bool found_violation = false;
            for (int b = 0; b < feature_bits.size() - 1; ++b)
            {
                if (feature_bits[b] == 1 && feature_bits[b + 1] == 0)
                {
                    found_violation = true;
                    if (subproblem_id >= 0)
                    {
                        std::cout << "VIOLATION in subproblem " << subproblem_id
                                  << ", feature " << feature
                                  << ", bit position " << b << "->" << (b + 1)
                                  << ": 1 followed by 0" << std::endl;
                        std::cout << "Feature bits: ";
                        for (int val : feature_bits)
                            std::cout << val;
                        std::cout << std::endl;
                    }
                    all_valid = false;
                }
            }

            bit_index += bits_for_feature;
        }

        return all_valid;
    }
}