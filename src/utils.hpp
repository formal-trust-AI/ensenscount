#ifndef UTILS_HPP
#define UTILS_HPP

#include <set>
#include <string>
#include <getopt.h>
#include <queue>
#include <iterator>
#include <map>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "cuddObj.hh"
#include "json.hpp"
#include "unordered_set"
using namespace std;

struct TreeNode
{
    int nodeid;
    int feature;
    double split_condition;
    double leaf;
    int upLeaf;
    int downLeaf;
    bool is_leaf;
    TreeNode *yes;
    TreeNode *no;
    int treeid;
    TreeNode(int id, int s, double sc, TreeNode *y, TreeNode *n)
        : nodeid(id),
          feature(s),
          split_condition(sc),
          leaf(0.0),
          upLeaf(0),
          downLeaf(0),
          is_leaf(false),
          yes(y),
          no(n) {}

    TreeNode(int id, double l)
        : nodeid(id),
          feature(-1),
          split_condition(0.0),
          leaf(l),
          upLeaf(static_cast<int>(l)),
          downLeaf(static_cast<int>(l)),
          is_leaf(true),
          yes(nullptr),
          no(nullptr) {}

    TreeNode(int id, double l, int ub, int lb)
        : nodeid(id),
          feature(-1),
          split_condition(0.0),
          leaf(l),
          upLeaf(ub),
          downLeaf(lb),
          is_leaf(true),
          yes(nullptr),
          no(nullptr) {}

    ~TreeNode()
    {
        if (yes)
            delete yes;
        if (no)
            delete no;
    }
};

void printBitMask(const vector<bool> &mask);
void parseDT(TreeNode *node);

// boolvar structure to represent splits (feature + threshold)
struct boolvar
{
    int feature;
    double split_val;

    boolvar(int f, double s) : feature(f), split_val(s) {}

    bool operator==(const boolvar &other) const
    {
        return feature == other.feature && split_val == other.split_val;
    }
};

// Hash function for boolvar to use in unordered_map/set
struct boolvarHash
{
    size_t operator()(const boolvar &bv) const
    {
        return hash<int>()(bv.feature) ^ (hash<double>()(bv.split_val) << 1);
    }
};
struct BoolVarComparator
{
    bool operator()(const boolvar &a, const boolvar &b) const
    {
        if (a.feature != b.feature)
            return a.feature < b.feature;
        return a.split_val < b.split_val;
    }
};

using json = nlohmann::json;
class bvDependency
{
public:
    boolvar lower;
    boolvar upper;
    bvDependency(boolvar l, boolvar u) : lower(l), upper(u) {}
    bool operator==(const bvDependency &other) const
    {
        return lower == other.lower && upper == other.upper;
    }
    bool operator<(const bvDependency &other) const
    {
        if (lower.feature != other.lower.feature)
            return lower.feature < other.lower.feature;
        return lower.split_val < other.lower.split_val;
    }
};
// Program configuration structure
struct ProgramConfig
{
    string jsonFilePath;
    string jsonFilePath2;
    int precision;
    double gap;
    int approx2;
    bool useDynamicOrdering;
    set<int> sensitiveFeatures;
    int bitDistance;
    int localConcurrency; // Maximum concurrent subproblems from this instance
    string logFileName;
    bool enableDebugOutput; // New option for debug output
    int debugSubproblemId;  // Which subproblem to debug (-1 for first subproblem, >=0 for specific ID)
    bool enableSanityCheck; // Enable sanity checking of assignments
    bool dumpAssignments;   // Dump satisfying assignments to file
    string dumpFileName;    // File name for dumping assignments
    // New scheduling parameters
    int timeoutSeconds;    // Timeout for individual subproblems in seconds (-1 for no timeout)
    int memoryLimitMB;     // Memory limit for individual subproblems in MB (-1 for no limit)
    int maxConcurrentJobs; // Maximum concurrent subproblems across ALL instances globally
    string splitsFilePath; // Path to splits file
    // Counting method selection
    string countingMethod; // "boundary", "naive", or "pepin" (default: "boundary")

    // Pepin algorithm parameters
    bool usePepinCounting;   // Whether to use Pepin approximation algorithm instead of exact counting
    double pepinEps;         // Error parameter for Pepin algorithm (default: 0.1)
    double pepinDelta;       // Confidence parameter for Pepin algorithm (default: 0.1)
    unsigned int randomSeed; // Random seed for reproducibility (0 = use random_device)

    // Secondary guard parameters
    double splitGap; // Gap for Secondary guards on non-sensitive features (0 = disabled)


    std::string additionalFilePath; // Path to an additional file

    // Verbosity level: 0 = quiet (errors only), 1 = normal, 2 = verbose, 3 = debug
    int verbosity;
};

// Global verbosity variable and logging macro
extern int g_verbosity;

#define VLOG(level) \
    if (::g_verbosity >= (level)) std::cout << "c o " << ((::g_verbosity >= 2) ? (std::string("[") + __func__ + "] ") : "") 

TreeNode *parseTreeNode(const json &node, int treeCount, int precision = -1);
TreeNode *parseTreeNodeUpDown(const json &node, int treeCount, int precision = -1);
TreeNode *parseTreeNodeOriginal(const json &node, int treeCount);
// Helper function to calculate BDD width using CountMinterm * 2
double calculateBDDWidth(const BDD &bdd, Cudd &manager);
double reverse_sigmoid(double y);
void printUsage(const char *programName);
ProgramConfig parseCommandLine(int argc, char *argv[]);

/**
 * Get the min and max values for each feature from the splits set
 * Returns a map from feature ID to pair<min_value, max_value>
 */
std::map<int, std::pair<double, double>> getFeatureRanges(
    const std::set<boolvar, BoolVarComparator> &splits);

/**
 * Generate Secondary guard splits at uniform intervals (splitGap) for non-sensitive features
 * and add them to the splits set
 * @param splits The set of splits to modify (will be updated with Secondary guards)
 * @param splitGap The gap size for Secondary guards (e.g., 0.1, 0.01)
 * @param sensitiveFeatures Set of sensitive feature IDs to exclude
 * @param precision Precision level for the model (for scaling purposes)
 */
void addSecondaryGuards(
    std::set<boolvar, BoolVarComparator> &splits,
    double splitGap,
    const std::set<int> &sensitiveFeatures,
    int precision, std::set<boolvar, BoolVarComparator> &ogSplits);

/**
 * Replace non-sensitive feature splits with the nearest Secondary guard values
 * This updates both the splits set and all tree nodes in the ensemble
 * @param splits The set of splits to modify
 * @param trees Vector of tree ensembles to update
 * @param sensitiveFeatures Set of sensitive feature IDs (which will NOT be replaced)
 */
void replaceWithNearestGuards(
    std::set<boolvar, BoolVarComparator> &splits,
    std::vector<TreeNode *> &trees,
    const std::set<int> &sensitiveFeatures);

void executeEnsemble(vector<TreeNode *> &ensemble, string f1, string f2);
// std::vector<TreeNode *> parseSecondModelFile(const std::string &filePath);

void printTree(TreeNode *node, int depth = 0);

void collectSplitsFromFile(
    const std::string &filePath,
    std::set<boolvar, BoolVarComparator> &splits);

#endif
