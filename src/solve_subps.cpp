#include "solve_subps.hpp"
#include "counting_wrapper.hpp"
#include "sanity_check.hpp"
#include <functional> // For std::function
#include <atomic>
#include <cassert>
#include <cmath>

using json = nlohmann::json;
using namespace std;

void print_bdd(const BDD &bdd)
{
    assert(bdd.getNode() != nullptr);
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

// Constructor Definition
SubproblemSolver::SubproblemSolver(Cudd mgr, int prec,
                                   std::map<boolvar, ADD, BoolVarComparator> &vMap,
                                   DebugOutputManager *debugMgr)
    : manager(mgr), precision(prec), varMap(vMap), debugManager(debugMgr) {}

// Member Function Definitions
set<bvDependency> SubproblemSolver::getRequiredConstraints(set<boolvar, BoolVarComparator> &VarSet)
{
    vector<boolvar> Vars(VarSet.begin(), VarSet.end());
    set<bvDependency> constraints;
    for (size_t i = 1; i < Vars.size(); i++)
    {
        if (Vars[i].feature != Vars[i - 1].feature)
            continue;
        constraints.insert(bvDependency(Vars[i - 1], Vars[i]));
    }
    return constraints;
}

ADD SubproblemSolver::get_dependency_constraint(boolvar &bv1, boolvar &bv2)
{
    assert(varMap.find(bv1) != varMap.end());
    assert(varMap.find(bv2) != varMap.end());
    ADD one_node = manager.addOne();
    ADD var1 = varMap.at(bv1);
    ADD var2 = varMap.at(bv2);
    ADD constraint = (var1.Xor(one_node)).Or(var2);
    return constraint;
}

void SubproblemSolver::applyRequiredConstraints(ADD &add,
                                                set<boolvar, BoolVarComparator> &yourVarSet,
                                                set<boolvar, BoolVarComparator> &varSet)
{
    set<bvDependency> yourPairs = getRequiredConstraints(yourVarSet);
    set<bvDependency> allPairs = getRequiredConstraints(varSet);
    for (auto dp : yourPairs)
        allPairs.erase(dp);
    for (auto dp : allPairs)
    {
        ADD constraint = get_dependency_constraint(dp.lower, dp.upper);
        add *= constraint;
    }
}

void SubproblemSolver::applyConstraints(ADD &add, set<boolvar, BoolVarComparator> &varSet)
{
    if (varSet.empty())
        return;
    set<bvDependency> constraints = getRequiredConstraints(varSet);
    for (auto dp : constraints)
    {
        ADD constraint = get_dependency_constraint(dp.lower, dp.upper);
        add *= constraint;
    }
}
void SubproblemSolver::applyConstraints(BDD &bdd, set<boolvar, BoolVarComparator> &varSet)
{
    if (varSet.empty())
        return;
    set<bvDependency> constraints = getRequiredConstraints(varSet);
    //cout << "# Applying " << constraints.size() << " dependency constraints." << endl;
    for (auto dp : constraints)
    {
        ADD constraint = get_dependency_constraint(dp.lower, dp.upper);
        bdd *= constraint.BddThreshold(0.5);
    }
}

ADD SubproblemSolver::convertToADD(TreeNode *tree, multiset<int> &featureSet,
                                   set<boolvar, BoolVarComparator> &varSet)
{
    assert(tree != nullptr);

    TreeNode *node = tree;
    function<ADD(TreeNode *)> build = [&](TreeNode *n) -> ADD
    {
        if (!n)
            return manager.addZero(); // Null check
        if (n->is_leaf)
            return manager.constant(n->leaf);
        assert(n->yes != nullptr);
        assert(n->no != nullptr);
        assert(varMap.find(boolvar(n->feature, n->split_condition)) != varMap.end());
        ADD cond = varMap[boolvar(n->feature, n->split_condition)];
        varSet.insert(boolvar(n->feature, n->split_condition));
        featureSet.insert(n->feature);
        ADD yesADD = build(n->yes);
        ADD noADD = build(n->no);
        return cond.Ite(yesADD, noADD);
    };

    ADD add = build(node);

    applyConstraints(add, varSet);
    // Precision handling is now done at TreeNode level, no need for RoundOff
    return add;
}

// Up and Down ADDs - Dec 12

ADD SubproblemSolver::upConvertToADD(TreeNode *tree,
                                     std::multiset<int> &featureSet,
                                     std::set<boolvar, BoolVarComparator> &varSet)
{
    assert(tree != nullptr);
    TreeNode *node = tree;

    std::function<ADD(TreeNode *)> build = [&](TreeNode *n) -> ADD
    {
        if (!n)
            return manager.addZero();

        if (n->is_leaf)
        {
            // cout<<n->upLeaf<<endl;
            return manager.constant(n->upLeaf);
        }

        boolvar bv(n->feature, n->split_condition);
        assert(n->yes != nullptr);
        assert(n->no != nullptr);
        assert(varMap.find(bv) != varMap.end());
        ADD cond = varMap[bv];
        varSet.insert(bv);
        featureSet.insert(n->feature);

        ADD yesADD = build(n->yes);
        ADD noADD = build(n->no);

        return cond.Ite(yesADD, noADD);
    };

    ADD add = build(node);

    applyConstraints(add, varSet);
    // Precision handling is now done at TreeNode level, no need for RoundOff
    return add;
}

ADD SubproblemSolver::downConvertToADD(TreeNode *tree,
                                       std::multiset<int> &featureSet,
                                       std::set<boolvar, BoolVarComparator> &varSet)
{
    assert(tree != nullptr);

    TreeNode *node = tree;

    // parseDT(node);

    std::function<ADD(TreeNode *)> build = [&](TreeNode *n) -> ADD
    {   
        if (!n)
            return manager.addZero();

        if (n->is_leaf)
        {
            // cout<<n->downLeaf<<endl;
            return manager.constant(n->downLeaf);
        }

        boolvar bv(n->feature, n->split_condition);
        assert(n->yes != nullptr);
        assert(n->no != nullptr);
        assert(varMap.find(bv) != varMap.end());
        ADD cond = varMap[bv];
        varSet.insert(bv);
        featureSet.insert(n->feature);

        ADD yesADD = build(n->yes);
        ADD noADD = build(n->no);

        return cond.Ite(yesADD, noADD);
    };

    ADD add = build(node);

    applyConstraints(add, varSet);
    // Precision handling is now done at TreeNode level, no need for RoundOff
    return add;
}

ADD SubproblemSolver::add_trees(vector<TreeNode *> &trees)
{

    if (trees.empty())
    {
        return manager.addZero();
    }

    // Convert & Collect
    vector<ADD> addList;
    addList.reserve(trees.size()); // Pre-allocate memory

    multiset<int> totalFeatureSet;
    set<boolvar, BoolVarComparator> totalVarSet;

    for (TreeNode *tree : trees)
    {
        assert(tree != nullptr);

        set<boolvar, BoolVarComparator> singleTreeVarSet;
        ADD singleAdd = convertToADD(tree, totalFeatureSet, singleTreeVarSet);
        addList.push_back(singleAdd);
        totalVarSet.insert(singleTreeVarSet.begin(), singleTreeVarSet.end());
    }

    // Apply Constraints
    for (ADD &add : addList)
    {

        applyConstraints(add, totalVarSet);
    }

    // Sum
    ADD totalSum = manager.addZero();
    for (const ADD &add : addList)
    {

        totalSum += add;
    }

    // Precision handling is now done at TreeNode level, no need for RoundOff

    return totalSum;
}

ADD SubproblemSolver::add_trees_with_debug(vector<TreeNode *> &trees, int ensembleId, const std::string &ensembleName)
{

    if (trees.empty())
    {
        return manager.addZero();
    }

    // Silence unused parameters in this overload
    (void)ensembleId;
    (void)ensembleName;

    // Convert & Collect
    vector<ADD> addList;
    addList.reserve(trees.size()); // Pre-allocate memory

    multiset<int> totalFeatureSet;
    set<boolvar, BoolVarComparator> totalVarSet;

    for (TreeNode *tree : trees)
    {
        assert(tree != nullptr);

        set<boolvar, BoolVarComparator> singleTreeVarSet;
        ADD singleAdd = convertToADD(tree, totalFeatureSet, singleTreeVarSet);
        addList.push_back(singleAdd);
        totalVarSet.insert(singleTreeVarSet.begin(), singleTreeVarSet.end());
    }

    // Apply Constraints
    for (ADD &add : addList)
    {

        applyConstraints(add, totalVarSet);
    }

    // Sum with intermediate debug output
    ADD totalSum = manager.addZero();
    for (size_t i = 0; i < addList.size(); ++i)
    {

        totalSum += addList[i];
    }


    return totalSum;
}

// Pairwise subtraction and summation
ADD SubproblemSolver::pairwise_subtract_and_sum(vector<TreeNode *> &ensemble1, vector<TreeNode *> &ensemble2, set<boolvar, BoolVarComparator> &varSet, int mySubproblemId)
{   
    assert(mySubproblemId >= 0);

    if (ensemble1.empty() || ensemble2.empty())
    {
        return manager.addZero();
    }

    // Ensure both ensembles have the same number of trees
    assert(ensemble1.size() == ensemble2.size());
    if (ensemble1.size() != ensemble2.size())
    {
        cerr << "Error: Ensembles must have the same number of trees for pairwise subtraction" << endl;
        return manager.addZero();
    }

    // Convert trees to ADDs and compute pairwise differences
    ADD diffSum = manager.addZero();
    multiset<int> totalFeatureSet;

    for (size_t i = 0; i < ensemble1.size(); ++i)
    {   
        assert(ensemble1[i] != nullptr);
        assert(ensemble2[i] != nullptr);
        set<boolvar, BoolVarComparator> singleTreeVarSet1;
        ADD add1 = upConvertToADD(ensemble1[i], totalFeatureSet, singleTreeVarSet1);
        varSet.insert(singleTreeVarSet1.begin(), singleTreeVarSet1.end());
        // Convert tree from ensemble2
        set<boolvar, BoolVarComparator> singleTreeVarSet2;
        ADD add2 = downConvertToADD(ensemble2[i], totalFeatureSet, singleTreeVarSet2);
        varSet.insert(singleTreeVarSet2.begin(), singleTreeVarSet2.end());
        // Apply constraints to both ADDs
        applyConstraints(add1, varSet);
        applyConstraints(add2, varSet);
        // Compute difference and add to sum
        ADD diff = add1 - add2;
        diffSum += diff;
    }

    return diffSum;
}

// Unified subproblem solving using counting wrapper
long long SubproblemSolver::solveSubproblem(pair<ADD, ADD> &add_pair, double gap, set<boolvar, BoolVarComparator> &varSet, int subproblemId)
{
    assert(subproblemId >= 0);
    assert(std::isfinite(gap));

    // Silence unused parameter when not used in non-debug flows
    (void)subproblemId;

    if (add_pair.first.IsZero())
    {
        cout << "Subproblem " << subproblemId << " first ADD is zero." << endl;
        return 0;
    }
    applyConstraints(add_pair.first, varSet);

    if (add_pair.second.IsZero())
    {
        cout << "Subproblem " << subproblemId << " second ADD is zero." << endl;
        return 0;
    }
    applyConstraints(add_pair.second, varSet);
    ADD diffadd = add_pair.first - add_pair.second;
    if (diffadd.IsZero())
    {
        cout << "Subproblem " << subproblemId << " ADD difference is zero." << endl;
        return 0;
    }
    BDD diff = (add_pair.first - add_pair.second).BddThreshold(gap);

    if (diff.IsZero())
    {
        cout << "Subproblem " << subproblemId << " BDD is zero after applying gap threshold." << endl;
        cout << "===============================" << endl;
        return 0;
    }

    // Perform sanity check on the final BDD if enabled

    // Use the counting wrapper with global configuration
    cout << "Solving Subproblem ID: " << subproblemId << endl;
    return CountingWrapper::countSol(diff, manager, gap);
}

ADD SubproblemSolver::add_trees_with_debug_for_subproblem(vector<TreeNode *> &trees, int ensembleId, const std::string &ensembleName, int subproblemId)
{
    assert(ensembleId >= 0);
    assert(subproblemId >= 0);

    if (trees.empty())
    {
        return manager.addZero();
    }

    // Convert & Collect
    vector<ADD> addList;
    addList.reserve(trees.size()); // Pre-allocate memory

    multiset<int> totalFeatureSet;
    set<boolvar, BoolVarComparator> totalVarSet;

    for (TreeNode *tree : trees)
    {
        assert(tree != nullptr);

        set<boolvar, BoolVarComparator> singleTreeVarSet;
        ADD singleAdd = convertToADD(tree, totalFeatureSet, singleTreeVarSet);
        addList.push_back(singleAdd);
        totalVarSet.insert(singleTreeVarSet.begin(), singleTreeVarSet.end());
    }

    // Apply Constraints
    for (ADD &add : addList)
    {

        applyConstraints(add, totalVarSet);
    }

    // Sum with intermediate debug output
    ADD totalSum = manager.addZero();
    for (size_t i = 0; i < addList.size(); ++i)
    {

        totalSum += addList[i];

        // Export intermediate ADD after each tree addition with explicit subproblem ID
        if (debugManager && debugManager->isEnabled())
        {
            debugManager->exportIntermediateADD(totalSum, manager, static_cast<int>(i), ensembleId, ensembleName, subproblemId);
        }
    }

    // Precision handling is now done at TreeNode level, no need for RoundOff

    return totalSum;
}

long long SubproblemSolver::solveSubproblemWithDebugForId(const std::vector<TreeNode *> &ensemble1,
                                                          const std::vector<TreeNode *> &ensemble2,
                                                          std::pair<ADD, ADD> &add_pair,
                                                          double gap,
                                                          std::set<boolvar, BoolVarComparator> &varSet,
                                                          int subproblemId)
{
    assert(subproblemId >= 0);
    assert(std::isfinite(gap));
    assert(ensemble1.size() == ensemble2.size());
    // Export debug information if enabled, using explicit subproblem ID
    if (debugManager && debugManager->isEnabled())
    {
        debugManager->exportSubproblemDataForId(ensemble1, ensemble2, add_pair.first, add_pair.second, manager, subproblemId);
    }

    // Call the regular solve function

    return solveSubproblem(add_pair, gap, varSet, subproblemId);
}
