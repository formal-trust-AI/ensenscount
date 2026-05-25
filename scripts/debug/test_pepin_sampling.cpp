#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <random>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <unordered_map>
#include "cuddObj.hh"
#include "pepin_counting.hpp"

using namespace std;

// Wrapper that mimics PepinBDDCounter's internal methods for testing
class TestPepinCounter
{
public:
    DdManager *mgr_ptr = nullptr;
    unordered_map<DdNode *, double> node_values;

    void set_manager(DdManager *mgr) { mgr_ptr = mgr; }

    // Precompute - uses same algorithm as pepin_counting.cpp
    double precompute_values(const BDD &bdd)
    {
        mgr_ptr = bdd.manager();
        node_values.clear();
        return dfs_precompute(bdd.getNode());
    }

    double dfs_precompute(DdNode *node)
    {
        // Handle complemented edges - get regular node
        bool is_complemented = Cudd_IsComplement(node);
        DdNode *regular_node = Cudd_Regular(node);

        auto it = node_values.find(regular_node);
        if (it != node_values.end())
        {
            double prob = it->second;
            return is_complemented ? (1.0 - prob) : prob;
        }

        if (Cudd_IsConstant(regular_node))
        {
            DdNode *mgr_const1 = Cudd_ReadOne(mgr_ptr);
            double v = (regular_node == mgr_const1) ? 1.0 : 0.0;
            node_values[regular_node] = v;
            return is_complemented ? (1.0 - v) : v;
        }

        DdNode *then_child = Cudd_T(regular_node);
        DdNode *else_child = Cudd_E(regular_node);

        double then_prob = dfs_precompute(then_child);
        double else_prob = dfs_precompute(else_child);
        double node_prob = 0.5 * (then_prob + else_prob);

        node_values[regular_node] = node_prob;
        return is_complemented ? (1.0 - node_prob) : node_prob;
    }

    // Sample - uses same algorithm as pepin_counting.cpp
    void sample_assignment(const BDD &bdd, vector<int> &assignment, mt19937 &rng)
    {
        fill(assignment.begin(), assignment.end(), -1);
        topdown_sample(bdd.getNode(), assignment, rng);
        uniform_int_distribution<int> coin_flip(0, 1);
        for (int i = 0; i < assignment.size(); ++i)
        {
            if (assignment[i] == -1)
            {
                assignment[i] = coin_flip(rng);
            }
        }
    }

    void topdown_sample(DdNode *node, vector<int> &assignment, mt19937 &rng)
    {
        // Handle complemented edges
        bool is_complemented = Cudd_IsComplement(node);
        DdNode *regular_node = Cudd_Regular(node);

        if (Cudd_IsConstant(regular_node))
        {
            return;
        }

        int var_index = Cudd_NodeReadIndex(regular_node);
        DdNode *then_child = Cudd_T(regular_node);
        DdNode *else_child = Cudd_E(regular_node);

        // If this node is complemented, swap the children's probabilities
        double then_prob = dfs_precompute(then_child);
        double else_prob = dfs_precompute(else_child);

        if (is_complemented)
        {
            then_prob = 1.0 - then_prob;
            else_prob = 1.0 - else_prob;
        }

        double total_prob = then_prob + else_prob;
        double prob_then = (then_prob / total_prob);

        uniform_real_distribution<double> dist(0.0, 1.0);
        double rand_val = dist(rng);

        if (rand_val < prob_then)
        {
            assignment[var_index] = 1;
            topdown_sample(is_complemented ? Cudd_Not(then_child) : then_child, assignment, rng);
        }
        else
        {
            assignment[var_index] = 0;
            topdown_sample(is_complemented ? Cudd_Not(else_child) : else_child, assignment, rng);
        }
    }

    // Print BDD structure with probabilities
    void print_bdd_structure(const BDD &bdd)
    {
        set<DdNode *> visited;
        function<void(DdNode *, int, string)> dfs = [&](DdNode *node, int depth, string edge)
        {
            for (int i = 0; i < depth; ++i)
                cout << "  ";
            if (!edge.empty())
                cout << edge << " ";

            if (visited.count(node))
            {
                cout << "[Node " << node << "] (visited)" << endl;
                return;
            }
            visited.insert(node);

            if (Cudd_IsConstant(node))
            {
                cout << "CONST " << (node == Cudd_ReadOne(mgr_ptr) ? "1" : "0") << endl;
                return;
            }

            int var_index = Cudd_NodeReadIndex(node);
            double prob = node_values.count(node) ? node_values[node] : -1.0;
            cout << "x" << var_index << " [prob=" << fixed << setprecision(4) << prob << "]" << endl;

            dfs(Cudd_T(node), depth + 1, "T->");
            dfs(Cudd_E(node), depth + 1, "E->");
        };
        dfs(bdd.getNode(), 0, "");
    }
};

// Test helper: evaluate BDD on assignment
bool evaluateBDD(const BDD &bdd, const vector<int> &assignment, Cudd &mgr)
{
    BDD result = bdd;
    for (size_t i = 0; i < assignment.size(); i++)
    {
        BDD var = mgr.bddVar(i);
        if (assignment[i] == 1)
        {
            result = result.Cofactor(var);
        }
        else if (assignment[i] == 0)
        {
            result = result.Cofactor(!var);
        }
    }
    return result != mgr.bddZero();
}

// Test helper: count exact solutions
long long countExact(const BDD &bdd, int num_vars)
{
    return (long long)bdd.CountMinterm(num_vars);
}

void printAssignment(const vector<int> &assignment)
{
    cout << "[";
    for (size_t i = 0; i < assignment.size(); i++)
    {
        if (i > 0)
            cout << " ";
        if (assignment[i] == -1)
            cout << "?";
        else
            cout << assignment[i];
    }
    cout << "]";
}

void runTest(const string &name, function<BDD(Cudd &, int)> buildBDD, int num_vars)
{
    cout << "\n========================================" << endl;
    cout << "Test: " << name << endl;
    cout << "Variables: " << num_vars << " (x0, x1, ..., x" << (num_vars - 1) << ")" << endl;
    cout << "========================================" << endl;

    Cudd mgr(0, 0);
    mgr.AutodynDisable();

    BDD bdd = buildBDD(mgr, num_vars);
    long long exact_count = countExact(bdd, num_vars);

    cout << "\n1. EXACT COUNT" << endl;
    cout << "   Satisfying assignments: " << exact_count << " / " << (1LL << num_vars) << endl;
    cout << "   Probability: " << (double)exact_count / (1LL << num_vars) << endl;

    // Test precomputation (from pepin_counting.cpp algorithm)
    TestPepinCounter counter;
    counter.set_manager(mgr.getManager());
    double computed_prob = counter.precompute_values(bdd);

    cout << "\n2. PRECOMPUTED PROBABILITIES (DFS)" << endl;
    cout << "   Root probability: " << fixed << setprecision(6) << computed_prob << endl;
    cout << "   Expected: " << (double)exact_count / (1LL << num_vars) << endl;
    cout << "   Match: " << (abs(computed_prob - (double)exact_count / (1LL << num_vars)) < 1e-9 ? "YES" : "NO") << endl;

    cout << "\n3. BDD STRUCTURE" << endl;
    counter.print_bdd_structure(bdd);

    // Test sampling (from pepin_counting.cpp algorithm)
    cout << "\n4. SAMPLING TEST (1000 samples)" << endl;
    mt19937 rng(42);
    int num_samples = 10000;
    int valid_samples = 0;
    map<vector<int>, int> sample_counts;

    for (int i = 0; i < num_samples; i++)
    {
        vector<int> assignment(num_vars);
        counter.sample_assignment(bdd, assignment, rng);

        if (evaluateBDD(bdd, assignment, mgr))
        {
            valid_samples++;
            sample_counts[assignment]++;
        }
    }

    cout << "   Valid samples: " << valid_samples << " / " << num_samples;
    cout << " (" << (100.0 * valid_samples / num_samples) << "%)" << endl;
    cout << "   Expected: ~100% (all samples should satisfy BDD)" << endl;

    if (exact_count <= 16)
    {
        cout << "\n5. SAMPLE DISTRIBUTION (showing all " << exact_count << " solutions)" << endl;
        cout << "   Assignment -> Count (expected ~" << (num_samples / exact_count) << " each)" << endl;
        for (const auto &[assignment, count] : sample_counts)
        {
            cout << "   ";
            printAssignment(assignment);
            cout << " -> " << count << " times" << endl;
        }
    }
    else
    {
        cout << "\n5. SAMPLE DISTRIBUTION" << endl;
        cout << "   Unique assignments sampled: " << sample_counts.size() << " / " << exact_count << endl;
        cout << "   Top 10 most frequent:" << endl;

        vector<pair<int, vector<int>>> sorted;
        for (const auto &[assignment, count] : sample_counts)
        {
            sorted.push_back({count, assignment});
        }
        sort(sorted.rbegin(), sorted.rend());

        for (int i = 0; i < min(10, (int)sorted.size()); i++)
        {
            cout << "   ";
            printAssignment(sorted[i].second);
            cout << " -> " << sorted[i].first << " times" << endl;
        }
    }

    cout << "\n========================================" << endl;
}

int main()
{
    cout << "PEPIN SAMPLING TESTER - Synthetic Examples" << endl;
    cout << "Testing precompute_values() and sample_assignment() from pepin_counting.cpp" << endl;
    cout << "==========================================" << endl;

    
    runTest("x0 AND NOT(x1 XOR x2 XOR x3)", [](Cudd &mgr, int n)
            {
                BDD x0 = mgr.bddVar(0);
                BDD x1 = mgr.bddVar(1);
                BDD x2 = mgr.bddVar(2);
                BDD x3 = mgr.bddVar(3);
                return x0 & !(x1 ^ x2 ^ x3); }, 4);

    cout << "\n==========================================" << endl;
    cout << "All tests completed!" << endl;
    cout << "==========================================" << endl;

    return 0;
}
