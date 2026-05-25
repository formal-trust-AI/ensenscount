#ifndef SOLVE_SUBPS_HPP
#define SOLVE_SUBPS_HPP

#include "utils.hpp"
#include <vector>
#include <set>
#include <map>
#include <utility> // For std::pair

// Forward declaration for the non-member function
int countSol(BDD &bdd, Cudd &manager, double gap,const std::vector<DdNode *> &yVars, bool quantifyOverYVars, bool useAbsoluteGap);
void enumerateAssignments(const BDD &bdd);
void dumpAssignments();
class SubproblemSolver
{
public:
    Cudd manager;
    int precision;
    std::map<boolvar, ADD, BoolVarComparator> varMap;
    std::set<string> cubes;
    // Constructor
    SubproblemSolver(Cudd mgr, int prec,
                     std::map<boolvar, ADD, BoolVarComparator> &vMap);

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

    ADD upConvertToADD(TreeNode *tree, std::multiset<int> &featureSet,
                     std::set<boolvar, BoolVarComparator> &varSet);   
                     
    ADD downConvertToADD(TreeNode *tree, std::multiset<int> &featureSet,
                     std::set<boolvar, BoolVarComparator> &varSet); 

    ADD add_trees(std::vector<TreeNode *> &trees);
    ADD add_uptrees(std::vector<TreeNode *> &trees);
    ADD add_downtrees(std::vector<TreeNode *> &trees);

    int solveSubproblem(std::pair<ADD, ADD> &add_pair, double gap, std::set<boolvar, BoolVarComparator> &varSet, const std::vector<DdNode *> &yVars);
    
    // Function to create constraints that sensitive guards cannot differ by more than k
    ADD createSensitiveGuardConstraints(const std::set<boolvar, BoolVarComparator> &sensitiveGuards,
                                        int k,
                                        const std::set<int> &sensitiveFeatures);
};

#endif // SOLVE_SUBPS_HPP