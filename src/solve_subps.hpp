#ifndef SOLVE_SUBPS_HPP
#define SOLVE_SUBPS_HPP

#include "gen_subps.hpp"
#include "debug_utils.hpp"
#include "counting_wrapper.hpp"

// Uses the counting wrapper for unified counting interface

class SubproblemSolver
{
public:
    Cudd manager;
    int precision;
    std::map<boolvar, ADD, BoolVarComparator> varMap;
    DebugOutputManager *debugManager; // Pointer to debug manager

    // Constructor
    SubproblemSolver(Cudd mgr, int prec,
                     std::map<boolvar, ADD, BoolVarComparator> &vMap,
                     DebugOutputManager *debugMgr = nullptr);

    // Member function declarations
    std::set<bvDependency> getRequiredConstraints(std::set<boolvar, BoolVarComparator> &VarSet);

    ADD get_dependency_constraint(boolvar &bv1, boolvar &bv2);

    void applyRequiredConstraints(ADD &add,
                                  std::set<boolvar, BoolVarComparator> &yourVarSet,
                                  std::set<boolvar, BoolVarComparator> &varSet);

    void applyConstraints(ADD &add, std::set<boolvar, BoolVarComparator> &varSet);
    void applyConstraints(BDD &bdd, std::set<boolvar, BoolVarComparator> &varSet);

    ADD convertToADD(TreeNode *tree, std::multiset<int> &featureSet,
                     std::set<boolvar, BoolVarComparator> &varSet);

    ADD upConvertToADD(TreeNode *tree,
                     std::multiset<int> &featureSet,
                     std::set<boolvar, BoolVarComparator> &varSet);

    ADD downConvertToADD(TreeNode *tree,
                     std::multiset<int> &featureSet,
                     std::set<boolvar, BoolVarComparator> &varSet);

    ADD add_trees(std::vector<TreeNode *> &trees);

    // New method for adding trees with debug output
    ADD add_trees_with_debug(std::vector<TreeNode *> &trees, int ensembleId, const std::string &ensembleName);

    // Method for adding trees with explicit subproblem ID
    ADD add_trees_with_debug_for_subproblem(std::vector<TreeNode *> &trees, int ensembleId, const std::string &ensembleName, int subproblemId);

    // Pairwise subtraction: subtract each tree in ensemble1 from corresponding tree in ensemble2, then sum
    ADD pairwise_subtract_and_sum(std::vector<TreeNode *> &ensemble1, std::vector<TreeNode *> &ensemble2, std::set<boolvar, BoolVarComparator> &varSet, int mySubproblemId);

    // Unified subproblem solving using counting wrapper
    long long solveSubproblem(std::pair<ADD, ADD> &add_pair, double gap, std::set<boolvar, BoolVarComparator> &varSet, int subproblemId = -1); // Helper function for debugging - returns the count for a specific subproblem
    long long solveSubproblemWithDebug(const std::vector<TreeNode *> &ensemble1,
                                       const std::vector<TreeNode *> &ensemble2,
                                       std::pair<ADD, ADD> &add_pair,
                                       double gap,
                                       std::set<boolvar, BoolVarComparator> &varSet);

    long long solveSubproblemWithDebugForId(const std::vector<TreeNode *> &ensemble1,
                                            const std::vector<TreeNode *> &ensemble2,
                                            std::pair<ADD, ADD> &add_pair,
                                            double gap,
                                            std::set<boolvar, BoolVarComparator> &varSet,
                                            int subproblemId);
};

#endif // SOLVE_SUBPS_HPP