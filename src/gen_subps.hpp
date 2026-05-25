#ifndef GEN_SUBPS_HPP
#define GEN_SUBPS_HPP

#include "utils.hpp"
#include <vector>
#include <set>
#include <map>
#include <utility> // For std::pair

// Forward declarations of standalone functions
int collectSplits(TreeNode *node, std::set<boolvar, BoolVarComparator> &splits, std::set<boolvar, BoolVarComparator> &treeSensSplits, std::set<int> &sensitiveFeatures);
int leafCount(TreeNode *node);
TreeNode *cloneTree(TreeNode *root);
std::vector<TreeNode *> cloneEnsemble(std::vector<TreeNode *> trees);
std::vector<std::vector<bool>> bitMaskGen(int k);
int bitDistance(std::vector<bool> v1, std::vector<bool> v2);
std::vector<std::vector<std::pair<std::vector<bool>, std::vector<bool>>>> subproblemGene(std::vector<std::vector<bool>> sensBitsMask, int dist);

struct PairHasher
{
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &p) const
    {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

TreeNode *PruneTreeMask(
    TreeNode *node,
    const std::unordered_map<std::pair<int, double>, bool, PairHasher> &bitmask);

TreeNode *PruneTreeAff(
    TreeNode *node,
    const std::set<boolvar, BoolVarComparator> &sensSplits);

struct bitMask
{
    std::vector<bool> mask;
    bitMask(int size) : mask(size, false) {}
};

// SubProblem Generator Class Declaration
class SubProblemGenerator
{
private:
    std::vector<TreeNode *> affectedTrees;
    std::vector<TreeNode *> unaffectedTrees;
    std::set<boolvar, BoolVarComparator> sensSplits;

public:
    // Constructor
    SubProblemGenerator(std::vector<TreeNode *> &inputTrees1, std::vector<TreeNode *> &inputTrees2,
                        std::set<boolvar, BoolVarComparator> &sensitiveSplits)
        : affectedTrees(inputTrees1), unaffectedTrees(inputTrees2), sensSplits(sensitiveSplits) {}

    // Function declaration (implementation goes in .cpp)
    std::vector<TreeNode *> pruneEnsemble(const std::vector<bool> &bitmask, vector<TreeNode *> unaffectedTrees, vector<TreeNode *> affectedTrees);

    // Getters (inlining is fine for simple functions)
    const std::vector<TreeNode *> &getTrees1() const { return affectedTrees; }
    const std::vector<TreeNode *> &getTrees2() const { return unaffectedTrees; }
    const std::set<boolvar, BoolVarComparator> &getSensitiveSplits() const { return sensSplits; }
};

#endif // GEN_SUBPS_HPP