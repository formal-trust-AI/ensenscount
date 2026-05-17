#include "utils.hpp"
#include "gen_subps.hpp"




// function to count number of leaves in tree

int leafCount(TreeNode *node)
{
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

TreeNode* PruneTreeMask(TreeNode* node, 
                                 const std::unordered_map<std::pair<int, double>, bool, PairHasher>& bitmask) {
    
    if (!node || node->is_leaf) {
        return node;
    }
    node->yes = PruneTreeMask(node->yes, bitmask);
    node->no = PruneTreeMask(node->no, bitmask);

    auto it = bitmask.find({node->feature, node->split_condition});

    if (it == bitmask.end()) {
        return node;
    }

    TreeNode* child_to_keep = nullptr;
    TreeNode* child_to_discard = nullptr;
    
    
    if (it->second) { 
        child_to_keep = node->yes;
        child_to_discard = node->no;
    } else { 
        child_to_keep = node->no;
        child_to_discard = node->yes;
    }

    node->yes = nullptr;
    node->no = nullptr;

    delete node;               
    delete child_to_discard;   

    return child_to_keep;
}

// function to clone tree

TreeNode *cloneTree(TreeNode *root)
{
    if (!root)
        return nullptr;

    if (root->is_leaf)
    {
        return new TreeNode(root->nodeid, root->leaf);
    }

    TreeNode *yesClone = cloneTree(root->yes);
    TreeNode *noClone = cloneTree(root->no);

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
        trees_clone.push_back(cloneTree(tr));
    }

    return trees_clone;
}

// bit mask generator - of k size

vector<vector<bool>> bitMaskGen(int k)
{
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

// function to count solutions to satisfying assignments



// Implementation of the pruneEnsemble member function
std::vector<TreeNode *> SubProblemGenerator::pruneEnsemble(const std::vector<bool> &bitmask)
{
    std::vector<TreeNode *> tree_ensemble = cloneEnsemble(trees);

    std::unordered_map<std::pair<int, double>, bool, PairHasher> bitmask2;

    int iterator = 0;
    for (const auto &sp : sensSplits)
    {
        if (iterator >= bitmask.size()) break; // Safety check
        
        std::pair<int, double> key = {sp.feature, sp.split_val};
        bitmask2[key] = bitmask[iterator];
        iterator++;
    }

    std::vector<TreeNode *> pruned_ensemble;
    for (const auto &dt : tree_ensemble)
    {   
        TreeNode *root = PruneTreeMask(dt, bitmask2);
        pruned_ensemble.push_back(root);
    }

    return pruned_ensemble;
}


vector<vector<pair<vector<bool>, vector<bool>>>> subproblemGene(vector<vector<bool>> sensBitsMask, int dist)
{
    std::vector<vector<std::pair<vector<bool>, vector<bool>>>> subpSet;
    std::vector<std::pair<vector<bool>, vector<bool>>> subpArray;

    for(int i=0;i<sensBitsMask.size();i++)
    {
        for(int j=0;j<sensBitsMask.size();j++){
            if((bitDistance(sensBitsMask[i],sensBitsMask[j])==0) || (bitDistance(sensBitsMask[i],sensBitsMask[j])>dist)){
                continue;
            }
            std::pair<vector<bool>, vector<bool>> maskp = {sensBitsMask[i], sensBitsMask[j]};
            subpArray.push_back(maskp);
        }

        // std::vector<std::pair<vector<bool>, vector<bool>>> subpArray;
        // for (int i = 0; i < sbm.size(); i++)
        // {
        //     for (int f = i + 1; f <= (i + dist); f++)
        //     {
        //         if (f == sbm.size())
        //         {
        //             break;
        //         }
        //         std::pair<vector<bool>, vector<bool>> maskp = {sbm[i], sbm[f]};
        //         subpArray.push_back(maskp);
        //     }
        // }
        // for (int j = (sbm.size() - 1); j >= 0; j--)
        // {
        //     for (int b = j - 1; b >= (j - dist); b--)
        //     {
        //         if (b < 0)
        //         {
        //             break;
        //         }
        //         std::pair<vector<bool>, vector<bool>> maskp = {sbm[j], sbm[b]};
        //         subpArray.push_back(maskp);
        //     }
        // }
        //subpSet.push_back(subpArray);
    }
    subpSet.push_back(subpArray);

    return subpSet;
}
