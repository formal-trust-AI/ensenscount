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
        : nodeid(id), feature(s), split_condition(sc), yes(y), no(n), is_leaf(false), leaf(0.0) {}

    TreeNode(int id, double l)
        : nodeid(id), feature(-1), split_condition(0.0), yes(nullptr), no(nullptr), is_leaf(true), leaf(l) {}

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
double reverse_sigmoid(double y);
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

void collectSplits(TreeNode *node, set<boolvar, BoolVarComparator> &splits);

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
    int precision;
    double gap;
    int approx2;
    bool useDynamicOrdering;
    set<int> sensitiveFeatures;
    int bitDistance;
    int max_procs;
    string logFileName;
};

TreeNode *parseTreeNode(const json &node, int treeCount, int precision = -1);
TreeNode *parseTreeNodeUpDown(const json &node, int treeCount, int precision = -1);
TreeNode *parseTreeNodeOriginal(const json &node, int treeCount);

void printUsage(const char *programName);
ProgramConfig parseCommandLine(int argc, char *argv[]);

#endif
