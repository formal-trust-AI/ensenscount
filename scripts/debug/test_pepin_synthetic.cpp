#include <iostream>
#include <vector>
#include <cmath>
#include <functional>
#include <fstream>
#include "cuddObj.hh"
#include "pepin_counting.hpp"

using namespace std;
int k;
  
// Test case structure
struct TestCase
{
    string name;
    function<vector<BDD>(Cudd &, int)> build_bdds; // Returns multiple BDDs
    long long expected_count;
    int num_vars;
};
vector<BDD> buildOrChainSubproblems(Cudd &mgr, int n)
{
    vector<BDD> bdds;
    for (int i = 0; i < n; i++)
    {
        bdds.push_back(mgr.bddVar(i));
    }
    return bdds;
}

vector<BDD> buildComplexDNFSubproblems(Cudd &mgr, int n)
{
    vector<BDD> bdds;
    int num_subproblems = pow(2, k);
    for (int i = 0; i < num_subproblems; i++)
    {
        cout<<"Building subproblem "<< (i+1) << "/" << num_subproblems << "..." <<endl;
        BDD subproblem = mgr.bddOne();
        // select an integer size for this subproblem randomly between 1 and n
        int size = rand() % n + 1;
        // sample 'size' variables randomly from the n variables
        // and take the AND of those variables
        for (int j = 0; j < size; j++)
        {
            int var_idx = (rand() % n);
            subproblem &= mgr.bddVar(var_idx);
        }
        bdds.push_back(subproblem);

    }

    return bdds;
}

vector<BDD> buildParitySubproblems(Cudd &mgr, int n)
{
    vector<BDD> bdds;

    if (n == 1)
    {
        bdds.push_back(mgr.bddVar(0));
        return bdds;
    }
    int num_subproblems = 1 << k;

    cout << "Building " << num_subproblems << " parity subproblems (k=" << k << ")..." << endl;

    // Generate DNF directly (not from BDD) to preserve constraints correctly
    vector<vector<vector<int>>> all_dnfs;

    for (int assignment = 0; assignment < num_subproblems; assignment++)
    {
        BDD constraint = mgr.bddOne();
        int parity = 0;
        vector<pair<int, bool>> fixed_vars;
        // Fix first k variables according to assignment
        // print the assignment in binary
        cout << "Subproblem " << (assignment + 1) << "/" << num_subproblems << ": assignment = ";
        for (int i = k - 1; i >= 0; i--)
        {
            bool bit = (assignment >> i) & 1;
            cout << bit;
        }
        cout << endl;
        for (int i = 0; i < k; i++)
        {
            bool bit = (assignment >> i) & 1;
            BDD var = mgr.bddVar(i);
            constraint &= bit ? var : !var;
            parity ^= bit;
            fixed_vars.push_back({i, bit});
        }

        BDD rest_xor = mgr.bddVar(k);
        for (int i = k + 1; i < n; i++)
        {
            rest_xor = rest_xor ^ mgr.bddVar(i);
        }
        cout<<"Parity is "<< (parity == 0 ? "0" : "1") << endl;
        BDD rest_constraint = (parity == 0) ? rest_xor : !rest_xor;
        constraint &= rest_constraint;
        bdds.push_back(constraint);

    }
    // print the bdds
    cout << "Constructed " << bdds.size() << " BDD subproblems." << endl;
    for (size_t i = 0; i < bdds.size(); i++)
    {
        cout << "Subproblem " << (i+1) << " BDD:" << endl;
        Cudd_PrintDebug(mgr.getManager(), bdds[i].getNode(), n, 4);
        cout << endl;
    }
    return bdds;
}

void runTest(TestCase &test, double eps = 0.05, double delta = 0.05, unsigned int seed = 4)
{
    cout << "\n========================================" << endl;
    cout << "Test: " << test.name << endl;
    cout << "Variables: " << test.num_vars << endl;
    cout << "Expected count: " << test.expected_count << endl;
    cout << "========================================" << endl;

    // Create CUDD manager
    Cudd mgr(0, 0);
    mgr.AutodynDisable();

    // Build the BDDs (multiple subproblems)
    vector<BDD> bdds = test.build_bdds(mgr, test.num_vars);

    cout << "Number of subproblems: " << bdds.size() << endl;

    // Verify exact count by taking OR of all BDDs
    BDD combined = mgr.bddZero();
    for (const auto &bdd : bdds)
    {
        combined |= bdd;
    }
    long long exact_from_bdd = (long long)combined.CountMinterm(test.num_vars);

    cout << "Exact count from BDD OR: " << exact_from_bdd << endl;
    cout << "Expected count (formula): " << test.expected_count << endl;

    if (test.expected_count == -1)
    {
        cout << "Expected count not specified, using BDD count as ground truth." << endl;
        test.expected_count = exact_from_bdd;
    }
    else if (exact_from_bdd != test.expected_count)
    {
        cout << "WARNING: BDD count doesn't match expected count!" << endl;
        cout << "Using BDD count as ground truth." << endl;
    }

    long long exact = exact_from_bdd; // Use BDD count as ground truth

    // Initialize Pepin with total number of subproblems
    PepinCounting::initGlobalPepin(eps, delta, seed, false, false);
    PepinCounting::setTotalSubproblems(bdds.size());

    // Create bitmasks (for single subproblem, both are same)
    vector<bool> bitmask(test.num_vars, 1);

    // Process each subproblem sequentially (simulating the real workflow)
    long long pepin_result = 0;
    for (size_t i = 0; i < bdds.size(); i++)
    {
        cout << "\nProcessing subproblem " << (i + 1) << "/" << bdds.size() << endl;
        pepin_result = PepinCounting::countSolutions(bdds[i], mgr, 2.0, bitmask, bitmask, eps, delta, seed);
    }

    // Get final count after all subproblems processed
    pepin_result = PepinCounting::getFinalGlobalCount();

    // Calculate error
    double error_percent = 0.0;
    if (exact > 0)
    {
        error_percent = 100.0 * abs(pepin_result - exact) / (double)exact;
    }

    cout << "\n--- RESULTS ---" << endl;
    cout << "Expected count (formula): " << test.expected_count << endl;
    cout << "Exact count (BDD):        " << exact << endl;
    cout << "Pepin count:              " << pepin_result << endl;
    cout << "Error vs exact:           " << (pepin_result - exact) << " (" << error_percent << "%)" << endl;

    // Check if within expected bounds
    double max_error = eps * exact;
    if (abs(pepin_result - exact) <= max_error)
    {
        cout << "PASS: Within epsilon bounds (±" << max_error << ")" << endl;
    }
    else
    {
        cout << "FAIL: Outside epsilon bounds (±" << max_error << ")" << endl;
    }

    // Clear state for next test
    PepinCounting::clearGlobalPepin();
}

int main(int argc, char **argv)
{
    // Test parameters
    double eps = 0.1;   // 10% error tolerance
    double delta = 0.1; // 90% confidence
    unsigned int seed = 4;
    int num_vars = 10;
    cout << "Enter number of variables for tests: ";
    cin >> num_vars;
    cout << "Enter the minimum number of subproblems for tests: ";
    int num_subproblems_input;

    cin >> num_subproblems_input;
    k = min((int)num_vars, (int)ceil(log2(num_subproblems_input)));
    // Define test cases with larger examples
    vector<TestCase> tests = {
        {"OR Chain",
         buildOrChainSubproblems,
         (1LL << num_vars) - 1, // 2^n - 1
         num_vars},
        {"Complex DNF with OR-separated subproblems",
         buildComplexDNFSubproblems,
         -1, // Will calculate from BDD
         num_vars}, // Fixed to 6 variables
        {"Parity XOR",
         buildParitySubproblems,
         (1LL << (num_vars - 1)), // 2^(n-1)
         num_vars}};

    // Run all tests
    int passed = 0;
    for (auto &test : tests)
    {
        try
        {
            runTest(test, eps, delta, seed);
            passed++;
        }
        catch (const exception &e)
        {
            cout << "✗ Test FAILED with exception: " << e.what() << endl;
        }
    }

    cout << "\n==================================================" << endl;
    cout << "Test Summary: " << passed << "/" << tests.size() << " completed" << endl;
    cout << "==================================================" << endl;

    return 0;
}
