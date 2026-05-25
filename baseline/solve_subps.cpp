#include "solve_subps.hpp"
#include <functional> // For std::function

using json = nlohmann::json;
using namespace std;
set<string> assignments;
// Constructor Definition
SubproblemSolver::SubproblemSolver(Cudd mgr, int prec,
                                   std::map<boolvar, ADD, BoolVarComparator> &vMap)
    : manager(mgr), precision(prec), varMap(vMap) {}

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
    for (auto dp : constraints)
    {
        ADD constraint = get_dependency_constraint(dp.lower, dp.upper);
        bdd *= constraint.BddThreshold(0.5);
    }
}

ADD SubproblemSolver::convertToADD(TreeNode *tree, multiset<int> &featureSet,
                                   set<boolvar, BoolVarComparator> &varSet)
{
    TreeNode *node = tree;
    function<ADD(TreeNode *)> build = [&](TreeNode *n) -> ADD
    {
        if (!n)
            return manager.addZero(); // Null check
        if (n->is_leaf)
            return manager.constant(n->leaf);
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

ADD SubproblemSolver::upConvertToADD(TreeNode *tree, multiset<int> &featureSet,
                                   set<boolvar, BoolVarComparator> &varSet)
{
    TreeNode *node = tree;
    function<ADD(TreeNode *)> build = [&](TreeNode *n) -> ADD
    {
        if (!n)
            return manager.addZero(); // Null check
        if (n->is_leaf)
            //return manager.constant(n->leaf);
            return manager.constant(n->upLeaf);
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

ADD SubproblemSolver::downConvertToADD(TreeNode *tree, multiset<int> &featureSet,
                                   set<boolvar, BoolVarComparator> &varSet)
{
    TreeNode *node = tree;
    function<ADD(TreeNode *)> build = [&](TreeNode *n) -> ADD
    {
        if (!n)
            return manager.addZero(); // Null check
        if (n->is_leaf)
            //return manager.constant(n->leaf);
            return manager.constant(n->downLeaf);
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

ADD SubproblemSolver::add_uptrees(vector<TreeNode *> &trees)
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
        set<boolvar, BoolVarComparator> singleTreeVarSet;
        ADD singleAdd = upConvertToADD(tree, totalFeatureSet, singleTreeVarSet);
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

ADD SubproblemSolver::add_downtrees(vector<TreeNode *> &trees)
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
        set<boolvar, BoolVarComparator> singleTreeVarSet;
        ADD singleAdd = downConvertToADD(tree, totalFeatureSet, singleTreeVarSet);
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

int SubproblemSolver::solveSubproblem(pair<ADD, ADD> &add_pair, double gap, set<boolvar, BoolVarComparator> &varSet, const std::vector<DdNode *> &yVars)
{
    applyConstraints(add_pair.first, varSet);
    applyConstraints(add_pair.second, varSet);
    BDD b1 = add_pair.first.BddThreshold(reverse_sigmoid(0.5+gap/2.0));
    BDD b2 = (-add_pair.second).BddThreshold(-reverse_sigmoid(0.5-gap/2.0));
    BDD diff = b1*b2;

    if (diff.IsZero())
        return 0;
    // return countSol(diff, manager, gap, yVars);
    return countSol(diff, manager, gap, yVars, true, true);
}
// int countSol(ADD &add, Cudd &manager, double gap)
// {
//     int satSol = 0;
// int countSol(ADD &add, Cudd &manager, double gap)
// {
//     int satSol = 0;

//     BDD gapBDD = add.BddThreshold(gap);
//     satSol = gapBDD.CountMinterm(manager.ReadSize());
//     BDD gapBDD = add.BddThreshold(gap);
//     satSol = gapBDD.CountMinterm(manager.ReadSize());

//     return satSol;
// }

// int countSol(ADD &add, Cudd &manager, double gap, const std::vector<DdNode *> &yVars) --V2
// {
//     BDD gapBDD = add.BddThreshold(gap);

//     if (!yVars.empty()) {
//         DdNode *cube = Cudd_bddComputeCube(manager.getManager(),
//                                            const_cast<DdNode**>(yVars.data()),
//                                            nullptr,
//                                            (int) yVars.size());
//         Cudd_Ref(cube);

//         DdNode *absNode = Cudd_bddExistAbstract(manager.getManager(),
//                                                 gapBDD.getNode(),
//                                                 cube);
//         Cudd_Ref(absNode);

//         BDD absBDD(manager, absNode);

//         Cudd_RecursiveDeref(manager.getManager(), cube);
//         Cudd_RecursiveDeref(manager.getManager(), absNode);

//         gapBDD = absBDD;
//     }

//     //debug section begins

//     //Cudd_PrintDebug(manager.getManager(),gapBDD.getNode(),manager.ReadSize(),4);

//     cout<<"Manager Variable Nodes :"<<endl;

//     for(int i=0;i<manager.ReadSize();i++){
//         cout<<manager.addVar(i).getNode()<<endl;
//     }

//     //debug section ends

//     BDD monotoneConstraint = manager.bddOne();
//     for (size_t i = 0; i + 1 < yVars.size(); ++i) {
//         BDD yi = BDD(manager, yVars[i]);
//         BDD yi1 = BDD(manager, yVars[i + 1]);
//         monotoneConstraint &= (!yi) | yi1;
//     }
//     gapBDD &= monotoneConstraint;

//     Cudd_PrintDebug(manager.getManager(),gapBDD.getNode(),manager.ReadSize(),4);

//     double sat = gapBDD.CountMinterm(manager.ReadSize());
//     return (long long) sat;
// }

int countSol(BDD &bdd, Cudd &manager, double gap,
             const std::vector<DdNode *> &yVars,
             bool quantifyOverYVars = true, // set false to quantify over all except yVars
             bool useAbsoluteGap = true)
{
    // Threshold to BDD (gap condition)
    BDD gapBDD;
    int totalVars = manager.ReadSize();

    
    gapBDD = bdd;
    

    // existential abstraction
    if (!yVars.empty())
    {
        std::vector<DdNode *> quantVars;

        if (quantifyOverYVars)
        {
            // Existentially eliminate yVars
            quantVars = yVars;
        }
        else
        {
            // Existentially eliminate all except yVars
            std::vector<DdNode *> allVars(totalVars);
            for (int i = 0; i < totalVars; i++)
            {
                allVars[i] = manager.bddVar(i).getNode();
            }

            // Compute complement: vars not in yVars
            std::unordered_set<DdNode *> ySet(yVars.begin(), yVars.end());
            for (auto v : allVars)
            {
                if (ySet.find(v) == ySet.end())
                    quantVars.push_back(v);
            }
        }

        if (!quantVars.empty())
        {
            DdNode *cube = Cudd_bddComputeCube(manager.getManager(),
                                               const_cast<DdNode **>(quantVars.data()),
                                               nullptr,
                                               (int)quantVars.size());
            Cudd_Ref(cube);

            DdNode *absNode = Cudd_bddExistAbstract(manager.getManager(),
                                                    gapBDD.getNode(),
                                                    cube);
            Cudd_Ref(absNode);

            gapBDD = BDD(manager, absNode);

            Cudd_RecursiveDeref(manager.getManager(), cube);
            Cudd_RecursiveDeref(manager.getManager(), absNode);
        }
    }

    // enumerateAssignments(gapBDD);
    // dumpAssignments();
    // Count satisfying assignments
    // DdNode *support = Cudd_Support(manager.getManager(), gapBDD.getNode());
    // int supportSize = Cudd_SupportSize(manager.getManager(), support);
    int supportSize = totalVars - yVars.size();
    cout << "Support size is: " << supportSize<<endl;
    // Cudd_RecursiveDeref(manager.getManager(), support);

    double sat = gapBDD.CountMinterm(supportSize);
    return (long long)sat;
}

ADD SubproblemSolver::createSensitiveGuardConstraints(const std::set<boolvar, BoolVarComparator> &sensitiveGuards,
                                                      int k,
                                                      const std::set<int> &sensitiveFeatures)
{
    ADD constraint = manager.addOne(); // Start with constraint that's always true

    if (k <= 0 || sensitiveGuards.empty())
    {
        return constraint; // No constraint if k is 0 or negative, or no sensitive guards
    }

    // Group sensitive guards by feature
    std::map<int, std::vector<boolvar>> guardsByFeature;
    for (const auto &guard : sensitiveGuards)
    {
        if (sensitiveFeatures.count(guard.feature) > 0)
        {
            guardsByFeature[guard.feature].push_back(guard);
        }
    }

    // For each sensitive feature, create constraints that limit the number of differing guards
    for (const auto &featurePair : guardsByFeature)
    {
        int feature = featurePair.first;
        const std::vector<boolvar> &guards = featurePair.second;

        if (guards.size() <= 1)
        {
            continue; // No constraint needed for features with 0 or 1 guards
        }

        // Sort guards by split value to ensure consistent ordering
        std::vector<boolvar> sortedGuards = guards;
        std::sort(sortedGuards.begin(), sortedGuards.end(),
                  [](const boolvar &a, const boolvar &b)
                  {
                      return a.split_val < b.split_val;
                  });

        // Create constraints that at most k guards can be different between two assignments
        // This ensures that we don't have more than k "transitions" in the guard values

        for (size_t i = 0; i < sortedGuards.size(); i++)
        {
            for (size_t j = i + k + 1; j < sortedGuards.size(); j++)
            {
                // Constraint: guards that are more than k positions apart should have same value

                if (varMap.find(sortedGuards[i]) != varMap.end() &&
                    varMap.find(sortedGuards[j]) != varMap.end())
                {

                    ADD var_i = varMap[sortedGuards[i]];
                    ADD var_j = varMap[sortedGuards[j]];

                    // Create constraint: (var_i == var_j) for guards that are k+1 apart
                    // This can be expressed as: (var_i AND var_j) OR (NOT var_i AND NOT var_j)
                    ADD both_true = var_i * var_j;
                    ADD both_false = (manager.addOne() - var_i) * (manager.addOne() - var_j);
                    ADD equality_constraint = both_true + both_false;

                    constraint *= equality_constraint;
                }
            }
        }
    }

    return constraint;
}
void enumerateAssignments(const BDD &bdd)
{

    long long total_assignments = 0;

    DdManager *manager = bdd.manager();

    if (manager == nullptr)
    {
        std::cout << "ERROR: BDD manager is null!" << std::endl;
        return;
    }

    int num_vars = Cudd_ReadSize(manager);
    std::cout << "BDD has " << num_vars << " variables" << std::endl;

    DdNode *bdd_node = bdd.getNode();
    if (bdd_node == nullptr)
    {
        std::cout << "ERROR: BDD node is null!" << std::endl;
        return;
    }

    // Use CUDD's cube enumeration to get all satisfying assignments
    DdGen *gen;
    int *cube;
    CUDD_VALUE_TYPE value;

    // Correct CUDD enumeration pattern - check if gen is not null
    long long total_individual_assignments = 0;
    int cubes_processed = 0;

    std::cout << "\nEnumerating cubes (each cube represents multiple assignments):" << std::endl;
    Cudd_ForeachCube(manager, bdd_node, gen, cube, value)
    {
        cubes_processed++;

        // Convert cube to assignment vector and count don't cares
        std::vector<int> cube_pattern(num_vars, -1);
        int dont_care_count = 0;
        for (int i = 0; i < num_vars; ++i)
        {
            if (cube[i] == 1)
            {
                cube_pattern[i] = 1; // Variable is set to true
            }
            else if (cube[i] == 0)
            {
                cube_pattern[i] = 0; // Variable is set to false
            }
            else
            {
                cube_pattern[i] = -1; // Don't care
                dont_care_count++;
            }
        }
        // Store the cube pattern for later analysis

        // std::vector<int> dontCarePositions;
        // for (int i = 0; i < num_vars; i++)
        // {
        //     if (cube[i] == 2) // Don't care
        //     {
        //         dontCarePositions.push_back(i);
        //     }
        // }
        // int numDontCares = dontCarePositions.size();
        // int totalAssignments = 1 << numDontCares; // 2^numDontCares
        // for (int assignment = 0; assignment < totalAssignments; assignment++)
        // {
        //     // Create a specific assignment for this iteration
        //     std::vector<bool> assignmentVector(num_vars, false);
        //     std::vector<bool> specificAssignment = assignmentVector;

        //     // Set don't care variables according to current assignment number
        //     for (int j = 0; j < numDontCares; j++)
        //     {
        //         int pos = dontCarePositions[j];
        //         bool bitValue = (assignment >> j) & 1;
        //         specificAssignment[pos] = bitValue;
        //     }
        //     string x;
        //     for (int i = 0; i < num_vars; i++) // Generate 41-bit assignments
        //     {

        //         // Use the specific assignment for sensitive features
        //         x += (specificAssignment[i] ? "1" : "0");
        //     }
        //     assignments.insert(x);
        // }
    }
}

void dumpAssignments()
{
    string filename = "baseline_assignments.txt";
    std::ofstream outfile(filename);
    if (!outfile.is_open())
    {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }
    outfile << "# Total unique assignments: " << assignments.size() << std::endl;
    size_t entry_idx = 0;
    for (const auto &entry : assignments)
    {
        if (entry.empty())
        {
            std::cerr << "[DEBUG] Warning: empty entry in globalSolutionSet at index " << entry_idx << std::endl;
        }
        outfile << entry << std::endl;
        ++entry_idx;
    }
    outfile.close();
}