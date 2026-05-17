
#include "../include/pepin_counting.hpp"
#include "utils.hpp"
#include <unordered_map>
#include <iostream>
#include <random>
#include <set>
#include <cmath>
#include <algorithm>
#include <cassert>
using namespace std;
// Forward declarations
bool evaluateBDD(Cudd &manager, const BDD &bdd, const std::vector<int> assignment);
// Step 3: Global sets and variables for Pepin counting
namespace
{
    // Global set X to store unique assignments and their bitmasks
    std::multiset<std::pair<std::vector<int>, std::vector<int>>> xassignments;
    // Final scaled count
    long long final_global_count = 0;
    double p = 1.0;             // Global probability factor (starts at 1)
    int total_subproblems = 0;  // m value - total number of subproblems
    int current_subproblem = 0; // Current subproblem index
    int threshold_value = 0;    // Thresh value from algorithm

    // Global Pepin parameters (initialized once)
    double global_eps = 0.1;
    double global_delta = 0.1;
    unsigned int global_seed = 42;
    bool pepin_initialized = false;
    bool debug_enabled = false; // Global debug flag
    // Global counter instance (now after class definition)
}

DdNode *buildCubeFromAssignment(Cudd &mgr, const std::vector<int> &assignment)
{
    assert(mgr.getManager() != nullptr);
    assert(assignment.size() <= static_cast<size_t>(mgr.ReadSize()));

    if (assignment.empty())
    {
        DdNode *cube = mgr.bddOne().getNode();
        Cudd_Ref(cube);
        return cube;
    }

    std::vector<DdNode *> vars;
    vars.reserve(assignment.size());
    for (size_t i = 0; i < assignment.size(); ++i)
    {
        assert(assignment[i] == 0 || assignment[i] == 1);
        vars.push_back(mgr.bddVar(i).getNode());
    }

    DdNode *cube = Cudd_bddComputeCube(mgr.getManager(),
                                       vars.data(),
                                       const_cast<int *>(assignment.data()),
                                       static_cast<int>(assignment.size()));
    Cudd_Ref(cube);
    return cube;
}

bool equalVectors(const std::vector<int> &a, const std::vector<int> &b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
            return false;
    }
    return true;
}
void printAssignment(const pair<vector<int>, vector<int>> pr)
{
    vector<int> passignment = pr.first;
    vector<int> bitmask = pr.second;
    cout << "Partial Assignment = ";
    cout << "[";
    for (size_t i = 0; i < passignment.size(); ++i)
    {
        cout << passignment[i];
    }
    cout << "]" << endl;

    cout << ", Bitmask = ";
    cout << "[";
    for (size_t i = 0; i < bitmask.size(); ++i)
    {
        cout << bitmask[i];
    }
    cout << "]" << endl;
}
class PepinBDDCounter
{
public:
    DdManager *mgr_ptr = nullptr;
    void set_manager(DdManager *mgr) { mgr_ptr = mgr; }

    void print_bdd(const BDD &bdd)
    {
        std::set<DdNode *> visited;
        std::function<void(DdNode *, int, std::string)> dfs = [&](DdNode *node, int depth, std::string edge)
        {
            for (int i = 0; i < depth; ++i)
                std::cout << "  ";
            if (!edge.empty())
                std::cout << edge << " ";
            if (visited.count(node))
            {
                std::cout << "[Node " << node << "] (visited)" << std::endl;
                return;
            }
            visited.insert(node);
            if (Cudd_IsConstant(node))
            {
                std::cout << "CONST " << Cudd_V(node) << std::endl;
                return;
            }
            int var_index = Cudd_NodeReadIndex(node);
            std::cout << "Node " << node << ": var=" << var_index << std::endl;
            dfs(Cudd_T(node), depth + 1, "T->");
            dfs(Cudd_E(node), depth + 1, "E->");
        };
        dfs(bdd.getNode(), 0, "");
    }
    std::unordered_map<DdNode *, double> node_values;
    double t_value; // Threshold value from boundary counting * 2

    // Returns the probability that this BDD node evaluates to TRUE
    double precompute_values(const BDD &bdd)
    {
        mgr_ptr = bdd.manager();
        assert(mgr_ptr != nullptr);
        DdNode *node = bdd.getNode();
        assert(node != nullptr);
        double prob = dfs_precompute(node);
        if (debug_enabled)
        {
            std::cout << "Precomputed BDD node probabilities:" << std::endl;
            for (const auto &kv : node_values)
            {
                std::cout << "  Node " << kv.first << ": " << kv.second << std::endl;
            }
        }
        return prob;
    }

    double dfs_precompute(DdNode *node)
    {
        // Handle complemented edges - get regular node
        bool is_complemented = Cudd_IsComplement(node);
        DdNode *regular_node = Cudd_Regular(node);
        assert(mgr_ptr != nullptr);
        assert(regular_node != nullptr);
        // Memoization check
        auto it = node_values.find(regular_node);
        if (it != node_values.end())
        {
            double prob = it->second;
            return is_complemented ? (1.0 - prob) : prob;
        }
        if (Cudd_IsConstant(regular_node))
        {
            DdNode *mgr_const1 = Cudd_ReadOne(mgr_ptr);
            double v;
            if (regular_node == mgr_const1)
            {
                v = 1.0;
            }
            else
            {
                v = 0.0; // Unknown constant node, treat as 0
            }
            node_values[regular_node] = v;
            return is_complemented ? (1.0 - v) : v;
        }
        DdNode *then_child = Cudd_T(regular_node);
        DdNode *else_child = Cudd_E(regular_node);

        double then_prob = dfs_precompute(then_child);
        double else_prob = dfs_precompute(else_child);
        double node_prob = 0.5 * (then_prob + else_prob);
        // Memoize and return
        node_values[regular_node] = node_prob;
        return is_complemented ? (1.0 - node_prob) : node_prob;
    }

    // Step 2: Generate a single sample using top-down traversal with precomputed probabilities
    void sample_assignment(const BDD &bdd, vector<int> &assignment, std::mt19937 &rng)
    {
        // Initialize all variables to -1 (unassigned)
        std::fill(assignment.begin(), assignment.end(), -1);
        DdNode *node = bdd.getNode();
        topdown_sample(node, assignment, rng);
        std::uniform_int_distribution<int> coin_flip(0, 1);
        for (int i = 0; i < assignment.size(); ++i)
        {
            if (assignment[i] == -1)
            {
                assignment[i] = coin_flip(rng);
            }
        }
    }

    void topdown_sample(DdNode *node, vector<int> &assignment, std::mt19937 &rng)
    {
        // Handle complemented edges
        bool is_complemented = Cudd_IsComplement(node);
        DdNode *regular_node = Cudd_Regular(node);

        // Base case: reached terminal node
        if (Cudd_IsConstant(regular_node))
        {
            return; // Assignment is complete when we reach terminal
        }

        // Get variable index for this node
        int var_index = Cudd_NodeReadIndex(regular_node);
        assert(var_index >= 0);
        assert(var_index < static_cast<int>(assignment.size()));

        // Get children
        DdNode *then_child = Cudd_T(regular_node);
        DdNode *else_child = Cudd_E(regular_node);

        // Get precomputed probabilities for children
        double then_prob = dfs_precompute(then_child);
        double else_prob = dfs_precompute(else_child);

        // If this node is complemented, swap the children's probabilities
        if (is_complemented)
        {
            then_prob = 1.0 - then_prob;
            else_prob = 1.0 - else_prob;
        }

        double total_prob = then_prob + else_prob;
        assert(total_prob > 0.0);
        double prob_then = (then_prob / total_prob);
        // now go to the then or else branch based on probability
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double rand_val = dist(rng);
        if (rand_val < prob_then)
        {
            // Take the 'then' branch - variable is true
            assignment[var_index] = 1;
            topdown_sample(is_complemented ? Cudd_Not(then_child) : then_child, assignment, rng);
        }
        else
        {
            // Take the 'else' branch - variable is false
            assignment[var_index] = 0;
            topdown_sample(is_complemented ? Cudd_Not(else_child) : else_child, assignment, rng);
        }
    }

    // Helper function to remove elements from X with given probability
    void removeWithProbability(std::multiset<std::pair<std::vector<int>, std::vector<int>>> &X, double prob, std::mt19937 &rng)
    {
        assert(prob >= 0.0 && prob <= 1.0);
        if (X.empty())
        {
            return;
        }
        VLOG(1) << "Size before removal: " << ::xassignments.size() << endl;
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        auto it = X.begin();
        while (it != X.end())
        {
            if (dist(rng) < prob)
            {
                it = X.erase(it);
            }
            else
            {
                ++it;
            }
        }
        VLOG(1) << "Size after removal: " << ::xassignments.size() << endl;
    }

    // Helper function to generate Poisson random number
    long long generatePoisson(double lambda, std::mt19937 &rng)
    {
        assert(std::isfinite(lambda));
        assert(lambda >= 0.0);
        std::poisson_distribution<long long> poisson(lambda);
        return poisson(rng);
    }

    long long generateBinomial(long long N, double p, std::mt19937 &rng)
    {
        assert(N >= 0);
        assert(p >= 0.0 && p <= 1.0);
        std::binomial_distribution<long long> binomial(N, p);
        return binomial(rng);
    }

    void clear_cache()
    {
        node_values.clear();
    }

    // Get the precomputed value for a node (for debugging)
    double get_node_value(DdNode *node) const
    {
        auto it = node_values.find(node);
        return (it != node_values.end()) ? it->second : -1.0;
    }

    // Step 4: Main counting algorithm using Pepin sampling
    long long processSubproblemPepin(const BDD &bdd, const vector<bool> &bitmask1, const vector<bool> &bitmask2,
                                     int threshold, int num_variables, unsigned int seed, Cudd &manager)
    {
        assert(threshold > 0);
        assert(num_variables >= 0);
        assert(bitmask1.size() == bitmask2.size());
        assert(p > 0.0 && p <= 1.0);
        assert(manager.getManager() != nullptr);

        threshold_value = threshold;
        current_subproblem++;
        // Step 4b: Precompute node values for sampling
        precompute_values(bdd);
        // Step 4c: Generate samples and collect unique assignments
        // Generate a unique seed for this subproblem
        unsigned int subproblem_seed = seed ^ (std::hash<int>{}(current_subproblem));
        std::mt19937 rng(subproblem_seed);
        std::uniform_int_distribution<int> coin_flip(0, 1);

        if (debug_enabled)
        {
            std::cout << "Starting Pepin algorithm for subproblem (assignment length: " << num_variables << ")" << std::endl;
        }

        // Implement the Pepin algorithm from the paper
        // Step 4: t ← 2^(n-width(Fi))
        double bdd_width = calculateBDDWidth(bdd, manager);
        double t = calculateBDDWidth(bdd, manager);
        assert(std::isfinite(bdd_width) && bdd_width >= 0.0);
        assert(std::isfinite(t) && t >= 0.0);

        if (debug_enabled)
        {
            std::cout << "BDD width: " << bdd_width << ", t: " << t << std::endl;
        }

        // Step 5-6: Remove elements from X that satisfy Fi
        // This prevents double-counting: if a sample already satisfies this subproblem,
        // remove it before generating new samples for this subproblem

        for (auto it = ::xassignments.begin(); it != ::xassignments.end();)
        {
            const auto &assignment = it->first;

            // Convert bitmask1 and bitmask2 from vector<bool> to vector<int> for comparison
            std::vector<int> bitmask1_int(bitmask1.begin(), bitmask1.end());
            std::vector<int> bitmask2_int(bitmask2.begin(), bitmask2.end());
            bool result = evaluateBDD(manager, bdd, assignment);
            if (result && (equalVectors(it->second, bitmask1_int) || equalVectors(it->second, bitmask2_int)))
            {
                if (debug_enabled)
                {
                    std::cout << "Removed assignment due to satisfying BDD: ";
                    printAssignment(*it);
                }
                it = ::xassignments.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // {
        //     while (p >= (double)threshold_value / t)
        //     {
        //         // Remove every element of X with prob. 1/2

        //         removeWithProbability(::xassignments, 0.5, rng);
        //         p = p / 2.0;
        //         cout << "Adjusted p to: " << p << endl;
        //         if (debug_enabled)
        //         {
        //             std::cout << "Reduced p to: " << p << ", X size: " << ::xassignments.size() << std::endl;
        //         }
        //     }
        // }

        VLOG(1) << "After filtering, X size: " << ::xassignments.size() << std::endl;

        // Step 10: Take Poisson(t * p) samples
        // int Ni = t * p;
        long long Ni = generatePoisson(t * p, rng);

        VLOG(1) << "Initial Ni = " << Ni << std::endl;

        int step = 0;
        // Step 11-13: While Ni + |X| > Thresh do
        while (Ni + (int)::xassignments.size() > threshold_value)
        {
            step++;
            VLOG(1) << "Step " << step << ": Ni = " << Ni << ", |X| = " << ::xassignments.size() << std::endl;
            // Remove every element of X with prob. 1/2
            removeWithProbability(::xassignments, 0.5, rng);

            // Update p = p/2 and recalculate Ni using Poisson
            p = p / 2.0;
            VLOG(1) << "Adjusted p to: " << p << std::endl;
            // Ni = t * p;
            // Ni = generatePoisson((t * p),rng);
            Ni = generateBinomial(Ni, 0.5, rng); // Jan 18 correction

            if (debug_enabled)
            {
                VLOG(1) << "Adjusted: p=" << p << ", Ni=" << Ni << ", |X|=" << ::xassignments.size() << std::endl;
            }
        }

        VLOG(1) << "Final Ni for this subproblem: " << Ni << std::endl;

        // Step 14-15: S ← GenerateSamples(Fi, Ni); X.Append(S)
        // Store each sample as a pair: (assignment, bitmask)
        std::vector<std::pair<std::vector<int>, std::vector<int>>> new_samples;
        for (int i = 0; i < Ni; ++i)
        {
            std::vector<int> assignment(num_variables, -1);
            sample_assignment(bdd, assignment, rng);
            for (int bit : assignment)
            {
                assert(bit == 0 || bit == 1);
            }
            std::vector<int> bitmask;
            double rand_val = coin_flip(rng);
            if (rand_val > 0.5)
            {
                bitmask.assign(bitmask1.begin(), bitmask1.end());
            }
            else
            {
                bitmask.assign(bitmask2.begin(), bitmask2.end());
            }
            // bitmask.assign(bitmask1.begin(), bitmask1.end());
            new_samples.emplace_back(assignment, bitmask);
            if (debug_enabled)
            {
                VLOG(1) << "Sample " << i + 1;
                printAssignment({assignment, bitmask});
            }
        }

        // Add new samples to X
        {
            int initial_size = ::xassignments.size();
            for (const auto &sample_pair : new_samples)
            {
                ::xassignments.insert(sample_pair);
            }
            int final_size = ::xassignments.size();
            if (debug_enabled)
            {
                int duplicates_found = new_samples.size() - (final_size - initial_size);
                if (duplicates_found > 0)
                {
                    VLOG(1) << "Found " << duplicates_found << " duplicate assignments out of "
                              << new_samples.size() << " generated samples" << std::endl;
                }
            }
        }

        // Final reporting
        int xassignments_size;
        static int prev_xassignments_size = 0;
        {
            xassignments_size = ::xassignments.size();
        }
        int delta_count = xassignments_size - prev_xassignments_size;
        prev_xassignments_size = xassignments_size;
        VLOG(1) << "Processed subproblem " << current_subproblem
                  << ": count_delta=" << delta_count
                  << ", total_count=" << xassignments_size << std::endl;
        return xassignments_size;
    }
};

bool evaluateBDD(Cudd &manager, const BDD &bdd, const vector<int> assignment)
{
    assert(manager.getManager() != nullptr);
    assert(bdd.getNode() != nullptr);
    assert(assignment.size() <= static_cast<size_t>(manager.ReadSize()));
    DdNode *cube = buildCubeFromAssignment(manager, assignment);
    Cudd_Ref(cube);
    DdNode *result = Cudd_bddRestrict(manager.getManager(), bdd.getNode(), cube);
    Cudd_Ref(result);
    Cudd_RecursiveDeref(manager.getManager(), cube);

    bool istrue = result != Cudd_ReadLogicZero(manager.getManager());
    Cudd_RecursiveDeref(manager.getManager(), result);
    return istrue;
}

namespace
{
    PepinBDDCounter global_counter;
}
// Step 5: Namespace wrapper functions matching interface expected by counting_wrapper.cpp
namespace PepinCounting
{
    // Initialize global Pepin parameters
    void initGlobalPepin(double eps, double delta, unsigned int seed, bool enableDebug, bool enableSanityCheck)
    {
        assert(eps > 0.0);
        assert(delta > 0.0 && delta < 1.0);

        // Set global parameters
        global_eps = eps;
        global_delta = delta;
        global_seed = seed;
        debug_enabled = enableDebug;
        pepin_initialized = true;

        // Clear any existing state (but keep cache for reuse)
        ::xassignments.clear();
        final_global_count = 0;
        p = 1.0;
        current_subproblem = 0;
        // DON'T clear global_counter cache - let it accumulate across subproblems

        VLOG(1) << "Global Pepin initialized with eps=" << eps
                  << ", delta=" << delta
                  << ", seed=" << seed
                  << ", debug=" << (enableDebug ? "enabled" : "disabled")
                  << ", sanityCheck=" << (enableSanityCheck ? "enabled" : "disabled") << std::endl;
    }

    // Set the total number of subproblems (m value)
    void setTotalSubproblems(int m)
    {
        assert(m > 0);
        ::total_subproblems = m;
        VLOG(1) << "Set total subproblems m = " << m << std::endl;
    }

    // Main counting interface that matches the expected signature
    long long countSolutions(BDD &bdd, Cudd &manager, double gap,
                             const vector<bool> &bitmask1,
                             const vector<bool> &bitmask2,
                             double eps, double delta, unsigned int seed)
    {
        // Use global parameters if Pepin was initialized, otherwise use passed parameters
        double use_eps = pepin_initialized ? global_eps : eps;
        double use_delta = pepin_initialized ? global_delta : delta;
        unsigned int use_seed = pepin_initialized ? global_seed : seed;
        assert(bitmask1.size() == bitmask2.size());
        assert(use_eps > 0.0);
        assert(use_delta > 0.0 && use_delta < 1.0);
        assert(manager.getManager() != nullptr);

        int num_variables = manager.ReadSize();
        // use threshold max(12 · ln(24/𝛿 )/𝜀2 , 6(ln 6/𝛿 + ln 𝑚)
        // m is the total number of subproblems in the entire problem
        int m = total_subproblems > 0 ? total_subproblems : 1; // Default to 1 if not set

        int threshold = std::max(
            (int)(12 * log(24.0 / use_delta) / (use_eps * use_eps)),
            (int)(6 * (log(6.0 / use_delta) + log((double)m))));
        assert(threshold > 0);

        VLOG(1) << "Pepin Counting: gap=" << gap << ", eps=" << use_eps
                  << ", delta=" << use_delta << ", seed=" << use_seed
                  << ", threshold=" << threshold << ", t=" << calculateBDDWidth(bdd, manager)
                  << " (global_init=" << pepin_initialized << ")" << std::endl;

        global_counter.processSubproblemPepin(bdd, bitmask1, bitmask2, threshold, num_variables, use_seed, manager);

        // Calculate and store final result
        {
            assert(p > 0.0);
            final_global_count = ::xassignments.size() / p;
            VLOG(1) << "Set size |X|: " << ::xassignments.size() << std::endl;
            VLOG(1) << "Probability factor p: " << p << std::endl;
            VLOG(1) << "Final estimated count: " << final_global_count << std::endl;
        }
        return final_global_count;
    }

    // Get final scaled count
    long long getFinalGlobalCount()
    {
        return final_global_count;
    }

    // Clear global state
    void clearGlobalPepin()
    {

        ::xassignments.clear();
        final_global_count = 0;
        global_counter.clear_cache();
        p = 1.0;
        current_subproblem = 0;
        ::total_subproblems = 0;
        threshold_value = 0;
        VLOG(1) << "Pepin global state cleared" << std::endl;
    }
}
