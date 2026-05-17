#include "utils.hpp"
#include "gen_subps.hpp"
#include <atomic>
#include <cassert>

// Helper functions
int collectSplits(TreeNode *node, set<boolvar, BoolVarComparator> &splits, set<boolvar, BoolVarComparator> &treeSensSplits, set<int> &sensitiveFeatures)
{
    assert(node != nullptr);
    int flag = 0;
    if (!node->is_leaf)
    {
        assert(node->yes != nullptr);
        assert(node->no != nullptr);
        splits.insert(boolvar(node->feature, node->split_condition));
        if (sensitiveFeatures.find(node->feature) != sensitiveFeatures.end())
        {
            flag = 1;
            treeSensSplits.insert(boolvar(node->feature, node->split_condition));
        }

        int j = collectSplits(node->yes, splits, treeSensSplits, sensitiveFeatures);
        int k = collectSplits(node->no, splits, treeSensSplits, sensitiveFeatures);
        if ((j == 1) || (k == 1))
        {
            flag = 1;
        }
    }
    return flag;
}

// function to count number of leaves in tree

int leafCount(TreeNode *node)
{
    assert(node != nullptr);
    if (node->is_leaf)
    {
        return 1;
    }
    else
    {
        return leafCount(node->yes) + leafCount(node->no);
    }
}

// function to prune tree with mask

TreeNode *PruneTreeMask(TreeNode *node,
                        const std::unordered_map<std::pair<int, double>, bool, PairHasher> &bitmask)
{

    if (!node || node->is_leaf)
    {
        return node;
    }
    node->yes = PruneTreeMask(node->yes, bitmask);
    node->no = PruneTreeMask(node->no, bitmask);

    auto it = bitmask.find({node->feature, node->split_condition});

    if (it == bitmask.end()) 
    {
        return node;
    }

    TreeNode *child_to_keep = nullptr;
    TreeNode *child_to_discard = nullptr;

    if (it->second)
    {
        child_to_keep = node->yes;
        child_to_discard = node->no;
    }
    else
    {
        child_to_keep = node->no;
        child_to_discard = node->yes;
    }
    assert(child_to_keep != nullptr);
    assert(child_to_discard != nullptr);

    node->yes = nullptr;
    node->no = nullptr;

    delete node;
    delete child_to_discard;

    return child_to_keep;
}

TreeNode *PruneTreeAff(TreeNode *node, const set<boolvar, BoolVarComparator> &sensSplits)
{   
    if (!node || node->is_leaf)
    {
        return node;
    }

    if (sensSplits.find(boolvar(node->feature, node->split_condition)) != sensSplits.end())
    {   
        return node;
    }
    else {
        if((node->yes)->is_leaf){
            node->yes = nullptr;
        }
        else{
            node->yes = PruneTreeAff(node->yes, sensSplits);
        }

        if((node->no)->is_leaf){
            node->no = nullptr;
        }
        else{
            node->no = PruneTreeAff(node->no, sensSplits);
        }

        if(node->yes != nullptr && node->no != nullptr) {
            return node;
        }
        else if(node->yes != nullptr) {
            TreeNode* leaf = new TreeNode(-1, 0.0);
            node->no = leaf;
            return node;
        }
        else if(node->no != nullptr) {
            TreeNode* leaf = new TreeNode(-1, 0.0);
            node->yes = leaf;
            return node;
        }
        else {
            delete node;
            return nullptr;
        }
    }

}

// function to clone tree

TreeNode *cloneTree(TreeNode *root)
{

    if (!root)
        return nullptr;

    if (root->is_leaf)
    {
        // return new TreeNode(root->nodeid, root->leaf);
        return new TreeNode(root->nodeid, root->leaf, root->upLeaf, root->downLeaf); // Dec 15
    }
    assert(root->yes != nullptr);
    assert(root->no != nullptr);

    TreeNode *yesClone = cloneTree(root->yes);

    TreeNode *noClone = cloneTree(root->no);
    assert(yesClone != nullptr);
    assert(noClone != nullptr);
    TreeNode *newNode = new TreeNode(
        root->nodeid,
        root->feature,
        root->split_condition,
        yesClone,
        noClone);
    newNode->treeid = root->treeid; // Copy tree ID as well

    return newNode;
}

// function to clone ensemble

vector<TreeNode *> cloneEnsemble(vector<TreeNode *> trees)
{
    vector<TreeNode *> trees_clone;

    for (const auto &tr : trees)
    {
        assert(tr != nullptr);
        trees_clone.push_back(cloneTree(tr));
    }

    return trees_clone;
}

// bit mask generator - of k size

vector<vector<bool>> bitMaskGen(int k)
{
    assert(k >= 0);
    std::vector<std::vector<bool>> masklib;

    std::vector<bool> bitmask(k, true);

    masklib.push_back(bitmask);

    for (int i = 0; i < k; i++)
    {
        bitmask[i] = false;
        masklib.push_back(bitmask);
    }
    return masklib;
}

// function to calculate bit distance

int bitDistance(vector<bool> v1, vector<bool> v2)
{
    assert(v1.size() == v2.size());
    int distance = 0;
    for (int i = 0; i < v1.size(); i++)
    {
        if (v1[i] != v2[i])
        {
            distance++;
        }
    }
    return distance;
}

// Implementation of the pruneEnsemble member function
std::vector<TreeNode *> SubProblemGenerator::pruneEnsemble(const std::vector<bool> &bitmask, vector<TreeNode *> unaffectedTrees, vector<TreeNode *> affectedTrees)
{
    assert(bitmask.size() <= sensSplits.size());
    std::vector<TreeNode *> tree_ensemble = cloneEnsemble(affectedTrees);
    assert(tree_ensemble.size() == affectedTrees.size());

    std::unordered_map<std::pair<int, double>, bool, PairHasher> bitmask2;

    int iterator = 0;
    for (const auto &sp : sensSplits)
    {
        if (iterator >= bitmask.size())
            break; // Safety check

        std::pair<int, double> key = {sp.feature, sp.split_val};
        bitmask2[key] = bitmask[iterator];
        iterator++;
    }

    std::vector<TreeNode *> pruned_ensemble;
    for (const auto &dt : tree_ensemble)
    {
        assert(dt != nullptr);
        TreeNode *pruned_tree = PruneTreeAff(dt, sensSplits);
        assert(pruned_tree != nullptr);
        TreeNode *root = PruneTreeMask(dt, bitmask2);
        assert(root != nullptr);
        pruned_ensemble.push_back(root);
    }
    assert(pruned_ensemble.size() == tree_ensemble.size());

    return pruned_ensemble;
}

vector<vector<pair<vector<bool>, vector<bool>>>> subproblemGene(vector<vector<bool>> sensBitsMask, int dist)
{
    assert(dist >= 0);
    for (size_t i = 1; i < sensBitsMask.size(); ++i)
    {
        assert(sensBitsMask[i].size() == sensBitsMask[0].size());
    }
    std::vector<vector<std::pair<vector<bool>, vector<bool>>>> subpSet;
    std::vector<std::pair<vector<bool>, vector<bool>>> subpArray;

    for (int i = 0; i < sensBitsMask.size(); i++)
    {
        for (int j = 0; j < sensBitsMask.size(); j++)
        {
            if ((bitDistance(sensBitsMask[i], sensBitsMask[j]) == 0) || (bitDistance(sensBitsMask[i], sensBitsMask[j]) > dist))
            {
                continue;
            }
            std::pair<vector<bool>, vector<bool>> maskp = {sensBitsMask[i], sensBitsMask[j]};
            subpArray.push_back(maskp);
        }

    }
    subpSet.push_back(subpArray);

    return subpSet;
}
