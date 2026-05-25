#include "utils.hpp"
#include "solve_subps.hpp"
#include <ctime>
#include <cmath>
#include <iostream>
#include <fstream>
using namespace std;

#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

int main(int argc, char *argv[])

{
    ProgramConfig config = parseCommandLine(argc, argv);

    string logFileName = config.logFileName;

    double precision = config.precision;
    double gap = config.gap;

    // Scale gap by 10^precision if precision is positive
    if (precision > 0)
    {
        gap = gap * pow(10, precision);
    }
    int approx2 = config.approx2;
    set<boolvar, BoolVarComparator> splits;
    set<boolvar, BoolVarComparator> sensSplits;
    map<int, int> featureGuardCount;
    vector<TreeNode *> trees;
    vector<TreeNode *> trees_original;
    string dir;
    size_t lastSlash = config.jsonFilePath.find_last_of('/');
    size_t lastDot = config.jsonFilePath.find_last_of('.');

    string sensFeatList;

    int slashCount = 0;
    string benchmark;
    for (char c : config.jsonFilePath)
    {
        if (c == '/')
        {
            slashCount++;
        }
        if (slashCount == 3)
        {
            break;
        }
        if ((slashCount == 2) && (c != '/'))
        {
            benchmark += c;
        }
    }

    for (int feature : config.sensitiveFeatures)
    {
        sensFeatList = sensFeatList + "_" + to_string(feature);
    }

    if (lastSlash != string::npos)
    {
        dir = config.jsonFilePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
    }
    else
    {
        dir = config.jsonFilePath.substr(0, lastDot - lastSlash - 1);
    }

    cout << endl;
    cout << "Model name: " << dir << endl;
    cout << "Precision: " << precision << endl;
    cout << "Gap: " << gap << endl;

    if (logFileName.empty())
    {
        // Compose a smart name: modelname_gap_precision_bitdist_time.log
        char timebuf[32];
        time_t now = time(0);
        strftime(timebuf, sizeof(timebuf), "%Y%m%d-%H%M%S", localtime(&now));
        logFileName = "./logs/" + dir + "_gap_" + to_string(gap) + "_sens_" + sensFeatList + "_prec_" + to_string(precision) + "_bitd_" + to_string(config.bitDistance) + "_" + string(timebuf) + ".log";
    }
    time_t t1 = time(0);
    ifstream jsonFile(config.jsonFilePath);

    cout << "Sensitive features: ";
    for (int feature : config.sensitiveFeatures)
    {
        cout << feature << " ";
    }
    cout << endl;

    cout << "Loading the json file..." << endl;

    if (!jsonFile.is_open())
    {
        cerr << "Failed to open JSON file: " << config.jsonFilePath << endl;
        return 1;
    }

    json decisionTrees;
    jsonFile >> decisionTrees;
    cout << endl
         << "Parsing the json file..." << endl;
    if (!decisionTrees.is_array())
    {
        cerr << "Invalid JSON format: Expected an array of decision trees." << endl;
        return 1;
    }

    int treeCount = 0;
    for (const auto &treeJson : decisionTrees)
    {
        //TreeNode *root = parseTreeNode(treeJson, treeCount, precision);
        TreeNode *root = parseTreeNodeUpDown(treeJson, treeCount, precision);
        TreeNode *root_og = parseTreeNodeOriginal(treeJson, treeCount);
        if (!root)
        {
            cerr << "Error parsing tree." << endl;
            return 1;
        }
        trees.push_back(root);
        treeCount++;
    }

    cout << endl
         << "No. of trees in the ensemble: " << treeCount << endl;

    // Collect splits and create variables
    for (TreeNode *tree : trees)
    {
        collectSplits(tree, splits);
    }
    cout << endl;
    cout << "No. of splits in the ensemble: " << splits.size() << endl;

    for (const auto &sp : splits)
    {
        if (config.sensitiveFeatures.find(sp.feature) != config.sensitiveFeatures.end())
        {
            sensSplits.insert(boolvar(sp.feature, sp.split_val));
        }
    }

    for (const auto &sp : splits)
    {
        if (featureGuardCount.find(sp.feature) != featureGuardCount.end())
        {
            featureGuardCount[sp.feature]++;
        }
        else
        {
            featureGuardCount[sp.feature] = 1;
        }
    }

    for (const auto &fg : featureGuardCount)
    {
        cout << fg.first << " " << fg.second << endl;
    }
    cout << endl;

    // log file creation

    ofstream logFile(logFileName);
    if (logFile.is_open())
    {
        logFile << "Benchmark: " << benchmark << "\n";
        logFile << "Code: Baseline\n";
        logFile << "Model name: " << dir << "\n";
        logFile << "JSON file: " << config.jsonFilePath << "\n";
        logFile << "Precision: " << precision << "\n";
        logFile << "Gap: " << gap << "\n";
        logFile << "Bit distance: " << config.bitDistance << "\n";
        // logFile << "Debug output: " << (config.enableDebugOutput ? "enabled" : "disabled") << "\n";
        logFile << "Sensitive features: ";
        for (int feature : config.sensitiveFeatures)
            logFile << feature << " ";
        logFile << "\n";
        logFile << "Number of trees: " << treeCount << "\n";
        logFile << "Number of splits: " << splits.size() << "\n";
        logFile << "Number of guards per feature:\n";
        for (const auto &fg : featureGuardCount)
            logFile << "  Feature " << fg.first << ": " << fg.second << "\n";
        logFile.close();
    }

    // Solve the complete problem using approach similar to convert_to_add.cpp
    time_t startTime = time(0);

    // Create CUDD manager
    Cudd manager(0, 0, 1 << 22, 262144, 0); // 1048576 slots
    map<boolvar, ADD, BoolVarComparator> varMap;
    map<boolvar, ADD, BoolVarComparator> sensitiveVarMap;

    // Create variables for ALL splits (both sensitive and non-sensitive)
    for (const auto &sp : splits)
    {
        ADD add_var = manager.addVar();
        varMap[sp] = add_var;
    }

    // Create separate variables for sensitive features (for swapping)
    for (const auto &sp : sensSplits)
    {
        ADD add_var = manager.addVar();
        sensitiveVarMap[sp] = add_var;
    }

    cout << "Total variables created: " << varMap.size() << endl;
    cout << "Sensitive variables created: " << sensitiveVarMap.size() << endl;

    // Create solver
    SubproblemSolver solver(manager, precision, varMap);

    // Build ADDs for the ensemble
    cout << "Building ADD for ensemble..." << endl;
    //ADD sumADD = solver.add_trees(trees);
    ADD sumUpADD = solver.add_uptrees(trees);
    ADD sumDownADD = solver.add_downtrees(trees);

    cout << "Ensemble converted to ADD successfully" << endl;

    // Create variable swapping arrays (x -> original, y -> swapped for sensitive vars)
    vector<DdNode *> xVars, yVars;
    for (const auto &bv : sensSplits)
    {
        if (varMap.find(bv) != varMap.end() && sensitiveVarMap.find(bv) != sensitiveVarMap.end())
        {
            // xVars.push_back(varMap[bv].getNode());
            // yVars.push_back(sensitiveVarMap[bv].getNode());
            int yIdx = Cudd_NodeReadIndex(sensitiveVarMap[bv].getNode());
            BDD yBddVar = manager.bddVar(yIdx);

            int xIdx = Cudd_NodeReadIndex(varMap[bv].getNode());
            BDD xBddVar = manager.bddVar(xIdx);

            xVars.push_back(xBddVar.getNode());
            yVars.push_back(yBddVar.getNode());
        }
    }

    // Create "at least one difference" constraint
    ADD atLeastOneDiff = manager.addZero();
    // for (const auto &bv : sensSplits)
    // {
    //     if (varMap.find(bv) != varMap.end() && sensitiveVarMap.find(bv) != sensitiveVarMap.end())
    //     {
    //         ADD var1 = varMap[bv];
    //         ADD var2 = sensitiveVarMap[bv];
    //         ADD diff = (var1 * (manager.addOne() - var2)) + ((manager.addOne() - var1) * var2);
    //         atLeastOneDiff += diff;
    //     }
    // }

    for (const auto &bv : sensSplits)
    {

        ADD xXORy = sensitiveVarMap[bv].Xor(varMap[bv]);
        atLeastOneDiff = atLeastOneDiff | xXORy;
    }

    if (atLeastOneDiff.IsZero())
    {
        cout << "Warning: No sensitive variables to compare!" << endl;
        return 1;
    }
    // else //commented Sep 11 22:33
    // {
    //     // convert atLeastOneDiff to be 1 where at least one difference exists
    //     BDD atLeastOneDiff_Bdd = atLeastOneDiff.BddThreshold(0.5);
    //     atLeastOneDiff = ADD(manager, atLeastOneDiff_Bdd.getNode());
    // }

    // Swap variables to create swapped
    // ADD swapped = ADD(manager,
    //                   Cudd_addSwapVariables(manager.getManager(), sumADD.getNode(),
    //                                         xVars.data(), yVars.data(), xVars.size()));

    ADD swappedUp = ADD(manager,
                      Cudd_addSwapVariables(manager.getManager(), sumUpADD.getNode(),
                                            xVars.data(), yVars.data(), xVars.size()));
    
    ADD swappedDown = ADD(manager,
                      Cudd_addSwapVariables(manager.getManager(), sumDownADD.getNode(),
                                            xVars.data(), yVars.data(), xVars.size()));
                                            

    cout << "Variable swapping completed" << endl;

    // Apply bit distance constraints to sensitive variables
    cout << "Applying bit distance constraints..." << endl;

    // Create bit distance constraint linking original and swapped variables
    ADD bitDistanceConstraint = manager.addOne();

    // Fix -2 for bitDistance

    if (config.bitDistance > 0 && !sensSplits.empty())
    {
        // Group sensitive guards by feature
        std::map<int, std::vector<boolvar>> guardsByFeature;
        for (const auto &guard : sensSplits)
        {
            if (config.sensitiveFeatures.count(guard.feature) > 0)
                guardsByFeature[guard.feature].push_back(guard);
        }

        // Collect diffs across ALL sensitive features
        std::vector<ADD> allDiffs;
        for (const auto &featurePair : guardsByFeature)
        {
            for (const auto &g : featurePair.second)
            {
                if (varMap.find(g) != varMap.end() &&
                    sensitiveVarMap.find(g) != sensitiveVarMap.end())
                {
                    allDiffs.push_back(varMap[g].Xor(sensitiveVarMap[g]));
                }
            }
        }

        if (!allDiffs.empty())
        {
            int k = config.bitDistance;
            int n = allDiffs.size();

            // dp[j] = ADD for "sum = j"
            std::vector<ADD> dp(k + 1, manager.addZero());
            dp[0] = manager.addOne();

            for (const auto &d : allDiffs)
            {
                std::vector<ADD> new_dp(k + 1, manager.addZero());
                for (int j = 0; j <= k; j++)
                {
                    // Case d = 0: sum stays j
                    new_dp[j] += dp[j] * (manager.addOne() - d);

                    // Case d = 1: sum increases by 1
                    if (j > 0)
                        new_dp[j] += dp[j - 1] * d;
                }
                dp.swap(new_dp);
            }

            // Constraint = "sum <= k"
            ADD featureConstraint = manager.addZero();
            for (int j = 0; j <= k; j++)
                featureConstraint += dp[j];

            bitDistanceConstraint *= featureConstraint;
        }
    }

    // Fix -2 for bitDistance

    // Apply the constraint to swapped

    // Calculate the difference

    // BDD diff1 = ((sumADD - swapped).BddThreshold(gap));
    // BDD diff2 = (swapped -sumADD).BddThreshold(gap);

    BDD diff1 = ((sumUpADD - swappedDown).BddThreshold(gap+1));
    BDD diff2 = (swappedDown -sumUpADD).BddThreshold(gap+1);
    BDD diff3 = ((sumDownADD - swappedUp).BddThreshold(gap+1));
    BDD diff4 = (swappedUp -sumDownADD).BddThreshold(gap+1);

    //BDD diff = diff1 | diff2;
    BDD diff = diff1 | diff2 | diff3 | diff4 ;
    if (!bitDistanceConstraint.IsOne())
    {
        cout << "Applying bit distance constraint (k=" << config.bitDistance << ")..." << endl;
        diff *= bitDistanceConstraint.BddThreshold(1);
        cout << "Bit distance constraint applied." << endl;
    }
    // Cudd_PrintDebug(manager.getManager(), diff.getNode(), manager.ReadSize(), 4);

    // Apply dependency constraints to the difference ADD
    cout << "Applying dependency constraints to difference ADD..." << endl;

    // Collect all variables used in the difference (original variables)
    set<boolvar, BoolVarComparator> originalVarSet;
    for (const auto &sp : splits)
    {
        originalVarSet.insert(sp);
    }

    // Apply dependency constraints to maxDiff for original variables
    solver.applyConstraints(diff, originalVarSet);

    // Apply dependency constraints for sensitive (swapped) variables
    if (!sensSplits.empty())
    {
        SubproblemSolver swapSolver(manager, precision, sensitiveVarMap);
        set<boolvar, BoolVarComparator> sensVarSet(sensSplits.begin(), sensSplits.end());
        swapSolver.applyConstraints(diff, sensVarSet);
    }

    cout << "Dependency constraints applied to difference ADD." << endl;

    // Apply the "at least one difference" constraint
    // diff *= atLeastOneDiff;

    cout << "Calculating satisfying assignments..." << endl;

    time_t endTime = time(0);
    double totalTime = difftime(endTime, startTime);

    cout << "Manager Variable Nodes (before countSol call) :" << endl;

    // for(int i=0;i<manager.ReadSize();i++){
    //     cout<<manager.addVar(i).getNode()<<endl;
    // }

    // use the countSol function
    // long long satisfyingAssignments = countSol(diff, manager, gap, yVars);
    long long satisfyingAssignments = countSol(diff, manager, gap, yVars, true, true);

    // cout<<"Variable to node mapping:"<<endl;

    // for (const auto &vm: varMap){
    //     cout<<vm.first.feature<<"<"<<vm.first.split_val<<" "<<vm.second.getNode()<<endl;
    // }

    // for (const auto &vm: sensitiveVarMap){
    //     cout<<vm.first.feature<<"<"<<vm.first.split_val<<" "<<vm.second.getNode()<<endl;
    // }

    // cout<<endl;

    cout << endl;
    cout << "Total satisfying assignments: " << satisfyingAssignments << endl;
    cout << "Time taken: " << totalTime << " seconds" << endl;

    // Write log file
    // ofstream logFile(logFileName);
    logFile.open(logFileName, ios::app);
    if (logFile.is_open())
    {
        logFile << "Total satisfying assignments: " << satisfyingAssignments << "\n";
        logFile << "Total time taken: " << totalTime << "\n";
        logFile.close();
        cout << "Log written to: " << logFileName << endl;
    }
    else
    {
        cerr << "Failed to write log file: " << logFileName << endl;
    }

    return 0;
}