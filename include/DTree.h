// TreeNode structure for parsed decision tree nodes
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
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
    bool is_leaf;
    TreeNode *yes;
    TreeNode *no;
    int treeid;
    TreeNode(int id, int s, double sc, TreeNode *y, TreeNode *n)
        : nodeid(id), feature(s), split_condition(sc), yes(y), no(n), is_leaf(false), leaf(0.0) {}

    TreeNode(int id, double l)
        : nodeid(id), feature(-1), split_condition(0.0), yes(nullptr), no(nullptr), is_leaf(true), leaf(l) {}

    ~TreeNode()
    {
        if (yes)
            delete yes;
        if (no)
            delete no;
    }
};

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
struct BoolVarComparator {
    bool operator()(const boolvar& a, const boolvar& b) const {
        if (a.feature != b.feature)
            return a.feature < b.feature;
        return a.split_val < b.split_val;
    }
};
