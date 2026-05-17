#include "utils.hpp"
#include "solve_subps.hpp"
#include "debug_utils.hpp"
#include "counting_naive.hpp"
#include "counting_config.hpp"
#include "counting_wrapper.hpp"
#include "pepin_counting.hpp"
#include "sanity_check.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <signal.h>
#include <atomic>
using namespace std;

//This is a comment 

vector<bool> bitvecXor(vector<bool> a, vector<bool> b){
    vector<bool> c(a.size());
    for(int i=0;i<a.size();i++){
        c[i]=a[i]^b[i];
    }
    return c;
}

vector<bool> bitvecAnd(vector<bool> a, vector<bool> b){
    vector<bool> c(a.size());
    for(int i=0;i<a.size();i++){
        c[i]=a[i]&b[i];
    }
    return c;
}

bool bitvecOr(vector<bool> a){
    bool k=0;
    for(int i=0;i<a.size();i++){
        k|=a[i];
    }
    return k;
}

int main(int argc, char *argv[])

{
    auto problemStartTime = std::chrono::steady_clock::now();
    ProgramConfig config = parseCommandLine(argc, argv);
    assert(!config.jsonFilePath.empty());
    assert(std::isfinite(config.precision));
    assert(std::isfinite(config.gap));
    string logFileName = config.logFileName;
    double precision = config.precision;
    double gap = config.gap;
    // Scale gap by 10^precision if precision is positive
    if (precision > 0)
    {
        gap = gap * pow(10, precision);
        VLOG(2) << "Scaled gap according to precision: " << gap << endl;
    }
    set<boolvar, BoolVarComparator> splits;
    set<boolvar, BoolVarComparator> sensSplits;
    map<int, int> featureGuardCount;
    vector<TreeNode *> trees;
    vector<TreeNode *> trees_original;
    vector<bool> affectedTreeIndex;
    vector<TreeNode *> unaffectedTrees;
    vector<TreeNode *> affectedTrees;
    string dir;
    size_t lastSlash = config.jsonFilePath.find_last_of('/');
    size_t lastDot = config.jsonFilePath.find_last_of('.');

    size_t lastSlash2 = config.jsonFilePath2.find_last_of('/');
    size_t lastDot2 = config.jsonFilePath2.find_last_of('.');

    // If second model file is provided, check that it has the same base name (up to last dot) as the first file
    if (config.jsonFilePath2 != "")
    {
        assert(lastDot2 != string::npos);
        assert(lastDot2 > lastSlash2 || lastSlash2 == string::npos);
        VLOG(1) << "Model 2 added." << endl;
    }

    string sensFeatList;

    // Extract benchmark name from the path (between 2nd and 3rd slash)
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

    // Append sensitive features to the log file name
    for (int feature : config.sensitiveFeatures)
    {
        sensFeatList = sensFeatList + "_" + to_string(feature);
    }

    // Extract directory name from the path (between last slash and last dot)
    if (lastSlash != string::npos)
    {
        assert(lastDot != string::npos);
        assert(lastDot > lastSlash);
        dir = config.jsonFilePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
    }
    else
    {
        assert(lastDot != string::npos);
        dir = config.jsonFilePath.substr(0, lastDot - lastSlash - 1);
    }

    VLOG(1) << "Model path :" << config.jsonFilePath << endl;
    VLOG(1) << "Model name: " << dir << endl;
    VLOG(1) << "Precision: " << precision << endl;
    VLOG(1) << "Gap: " << gap << endl;
    int targetSubproblemId = (config.debugSubproblemId >= 0) ? config.debugSubproblemId : 0;
    VLOG(1) << "Debug output: " << (config.enableDebugOutput ? ("enabled (limited to subproblem " + to_string(targetSubproblemId) + " only)") : "disabled") << endl;
    VLOG(1) << "Sanity checking: " << (config.enableSanityCheck ? "enabled" : "disabled") << endl;

    // Construct log file name if not provided
    if (logFileName.empty())
    {
        char timebuf[32]; // Buffer to hold formatted time
        time_t now = time(0);
        strftime(timebuf, sizeof(timebuf), "%Y%m%d-%H%M%S", localtime(&now));
        logFileName = "./logs/" + dir + "_gap_" + to_string(gap) + "_sens_" + sensFeatList + "_prec_" + to_string(precision) + "_bitd_" + to_string(config.bitDistance) + "_" + string(timebuf) + ".log";
    }
    {
        std::filesystem::path logPath(logFileName);
        if (logPath.has_parent_path())
        {
            std::error_code ec;
            std::filesystem::create_directories(logPath.parent_path(), ec);
            if (ec)
            {
                cerr << "Warning: failed to create log directory '" << logPath.parent_path().string()
                     << "': " << ec.message() << endl;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    VLOG(1) << "Time starts now" << endl;
    ifstream jsonFile(config.jsonFilePath);
    ifstream jsonFile2(config.jsonFilePath2);

    VLOG(1) << "Sensitive features: ";

    if(g_verbosity>=1){
        for (int feature : config.sensitiveFeatures)
        {
            if(g_verbosity>=1){
                cout<<feature<<" ";
            }
        }
        cout << endl;
    }

    VLOG(1) << "Counting method: " << config.countingMethod << endl;

    // Set global counting configuration
    CountingConfig::setConfig(config.countingMethod);

    // Set global Pepin configuration if using Pepin counting
    if (config.countingMethod == "pepin" || config.usePepinCounting)
    {
        CountingConfig::setPepinConfig(config.pepinEps, config.pepinDelta, config.randomSeed);
    }

    // Clear global naive solution set if using naive counting
    if (config.countingMethod == "naive")
    {
        CountingNaive::clearGlobalSet();
    }

    // Loading json file
    VLOG(1) << "Loading the json file..." << flush;
    if (!jsonFile.is_open())
    {
        cerr << "Failed to open JSON file: " << config.jsonFilePath << endl;
        return 1;
    }
    json decisionTrees;
    jsonFile >> decisionTrees;

    json decisionTrees2;
    if (config.jsonFilePath2 != "")
    {
        assert(jsonFile2.is_open());
        jsonFile2 >> decisionTrees2;
        assert(decisionTrees2.is_array());
    }
    if (!decisionTrees.is_array())
    {
        cerr << "Invalid JSON format: Expected an array of decision trees." << endl;
        return 1;
    }
    VLOG(1) << "Done" << endl;

    VLOG(2) << "List of leaf values :" << endl;


    int treeCount = 0;
    // Parse trees and build internal representation
    for (const auto &treeJson : decisionTrees)
    {
        TreeNode *root = parseTreeNodeUpDown(treeJson, treeCount, precision);
        TreeNode *root_og = parseTreeNodeOriginal(treeJson, treeCount);
        if (!root)
        {
            cerr << "Error parsing tree." << endl;
            return 1;
        }
        trees.push_back(root);
        if (config.enableDebugOutput)
        {
            assert(root_og != nullptr);
            trees_original.push_back(root_og);
        }
        treeCount++;
    }
    VLOG(1) << "No. of trees in the ensemble: " << treeCount << endl;


    vector<set<boolvar, BoolVarComparator>> treewiseSensSplits;

    // Collect splits and create variables
    for (TreeNode *tree : trees)
    {
        set<boolvar, BoolVarComparator> treeSensSplits;
        int flag = collectSplits(tree, splits, treeSensSplits, config.sensitiveFeatures);
        affectedTreeIndex.push_back(flag);
        treewiseSensSplits.push_back(treeSensSplits);
        // if(flag==1){
        //     treewiseSensSplits.push_back(treeSensSplits);
        // }
    }

    // for (const auto &tsp : treewiseSensSplits){
    //     for (const auto &sp : tsp){
    //         cout << sp.feature << " " << sp.split_val << endl;
    //     }
    //     cout<<endl<<endl;
    // }

    VLOG(1) << "No. of splits in the original ensemble: " << splits.size() << endl;

    // Identify splits on sensitive features
    for (const auto &sp : splits)
    {
        if (config.sensitiveFeatures.find(sp.feature) != config.sensitiveFeatures.end())
        {
            sensSplits.insert(boolvar(sp.feature, sp.split_val));
        }
    }
    VLOG(1) << "No. of sensitive feature splits in the original ensemble: " << sensSplits.size() << endl;


    vector<vector<bool>> treeSensVec;

    for(const auto &tsp : treewiseSensSplits){
        vector<bool> splitArray;
        for(const auto &sp : sensSplits){
            if(tsp.find(sp)!=tsp.end()){
                splitArray.push_back(true);
            }
            else splitArray.push_back(false); 
        }
        treeSensVec.push_back(splitArray);
    }

    // for (const auto &tsv : treeSensVec){
    //     for (const auto &sa : tsv){
    //         cout << sa << " ";
    //     }
    //     cout<<endl;
    // }

    // cout<<endl<<endl;

    if (!config.jsonFilePath2.empty())
    {
        VLOG(1) << "Parsing additional model file for trees: " << config.jsonFilePath2 << endl;
        ifstream jsonFile2(config.jsonFilePath2);
        if (!jsonFile2.is_open())
        {
            cerr << "Error: Unable to open the additional model file: " << config.jsonFilePath2 << endl;
            return 1;
        }
        json decisionTrees2;
        jsonFile2 >> decisionTrees2;

        std::vector<TreeNode *> secondModelTrees;
        for (const auto &treeJson : decisionTrees2)
        {

            TreeNode *root_og = parseTreeNodeOriginal(treeJson, treeCount);
            if (!root_og)
            {
                cerr << "Error parsing tree." << endl;
                return 1;
            }
            secondModelTrees.push_back(root_og);
        }
        vector<set<boolvar, BoolVarComparator>> treewiseSensSplits2;

        for (TreeNode *tree : secondModelTrees)
        {   
            set<boolvar, BoolVarComparator> treeSensSplits;
            int flag = collectSplits(tree, splits, treeSensSplits, config.sensitiveFeatures);
            if(flag==1){
                treewiseSensSplits2.push_back(treeSensSplits);
            }
        }
        VLOG(1) << "No. of splits after taking union from second model: " << splits.size() << endl;
    }

    if (!config.splitsFilePath.empty())
    {
        VLOG(1) << "Parsing splits from file: " << config.splitsFilePath << endl;
        collectSplitsFromFile(config.splitsFilePath, splits);
        VLOG(1) << "No. of splits after adding from splits file: " << splits.size() << endl;
    }

    // Add secondary guards for non-sensitive features if splitGap is specified
    if (config.splitGap > 0.0)
    {
        set<boolvar, BoolVarComparator> ogSplits = splits;

        addSecondaryGuards(splits, config.splitGap, config.sensitiveFeatures, precision, ogSplits);

        // Replace non-sensitive feature splits with nearest guard values
        replaceWithNearestGuards(splits, trees, config.sensitiveFeatures);
        VLOG(1) << "No. of unique splits after replacement: " << splits.size() << endl;
    }

    // print the updated trees
    VLOG(2) << "Updated Trees after Secondary guard replacement: " << endl;

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
        VLOG(1) << "Guard counts feature:" << fg.first << ", count: " << fg.second << endl;
    }

    std::vector<std::vector<std::vector<bool>>> sensBitMasks;

    for (const auto &fg : featureGuardCount)
    {
        if (config.sensitiveFeatures.find(fg.first) != config.sensitiveFeatures.end())
        {
            sensBitMasks.push_back(bitMaskGen(fg.second));
        }
    }
    std::vector<vector<std::pair<std::vector<bool>, std::vector<bool>>>> spArray;

    std::vector<std::vector<bool>> sensBitMasksComplete;

    for (const auto &sbm : sensBitMasks)
    {
        if (sensBitMasksComplete.empty())
        {
            for (const auto &bm : sbm)
            {
                sensBitMasksComplete.push_back(bm);
            }
            continue;
        }
        std::vector<std::vector<bool>> sensBitMasksComplete_copy;

        for (auto &sbmc : sensBitMasksComplete)
        {
            for (auto &bm : sbm)
            {

                std::vector<bool> appendMask;
                appendMask = sbmc;
                for (int i = 0; i < bm.size(); i++)
                {
                    appendMask.push_back(bm[i]);
                }
                sensBitMasksComplete_copy.push_back(appendMask);
            }
        }
        sensBitMasksComplete.clear();
        sensBitMasksComplete = sensBitMasksComplete_copy;
    }

    // Generate subproblems using the complete bitmask combinations
    spArray = subproblemGene(sensBitMasksComplete, config.bitDistance);
    for (const auto &sp : spArray)
    {
        for (const auto &pair : sp)
        {
            assert(pair.first.size() == pair.second.size());
        }
    }
    if (!spArray.empty())
    {
        VLOG(1) << "Subproblems generated: " << spArray[0].size() << endl
                  << endl;
    }
    else
    {
        VLOG(1) << "No subproblems generated." << endl
                  << endl;
    }

    for (int i = 0; i < affectedTreeIndex.size(); i++)
    {
        if (affectedTreeIndex[i] == 1)
        {
            affectedTrees.push_back(trees[i]);
        }
        else
        {
            unaffectedTrees.push_back(trees[i]);
        }
    }

    //VLOG(1) << "Unaffected Trees : " << unaffectedTrees.size() << endl;
    //VLOG(1) << "Affected Trees : " << affectedTrees.size() << endl;



    DebugOutputManager debugManager(config.enableDebugOutput, "debug_output");

    // Initialize sanity checking
    SanityCheck::enableSanityCheck(config.enableSanityCheck);

    // Export original ensemble if debug is enabled
    if (config.enableDebugOutput)
    {
        debugManager.exportOriginalEnsemble(trees_original);
        debugManager.exportScaledEnsemble(trees);
    }

    ofstream logFile(logFileName);
    if (logFile.is_open())
    {
        logFile << "Benchmark: " << benchmark << "\n";
        logFile << "Code: Subproblem division\n";
        logFile << "Model name: " << dir << "\n";
        logFile << "JSON file: " << config.jsonFilePath << "\n";
        logFile << "Precision: " << precision << "\n";
        logFile << "Gap: " << gap << "\n";
        logFile << "Bit distance: " << config.bitDistance << "\n";
        logFile << "Debug output: " << (config.enableDebugOutput ? ("enabled (limited to subproblem " + to_string(targetSubproblemId) + " only)") : "disabled") << "\n";
        if (config.enableDebugOutput)
        {
            logFile << "Debug subproblem ID: " << targetSubproblemId << "\n";
        }
        logFile << "Sensitive features: ";
        for (int feature : config.sensitiveFeatures)
            logFile << feature << " ";
        logFile << "\n";
        logFile << "Counting method: " << config.countingMethod << "\n";
        logFile << "Number of trees: " << treeCount << "\n";
        logFile << "Number of splits: " << splits.size() << "\n";
        logFile << "Number of guards per feature:\n";
        for (const auto &fg : featureGuardCount)
            logFile << "  Feature " << fg.first << ": " << fg.second << "\n";
        logFile.close();
    }

    SubProblemGenerator subProblemGen(affectedTrees, unaffectedTrees, sensSplits); 
    std::vector<std::tuple<long long, double, double>> results;
    int subproblemCounter = 0;
    auto solve_subproblem = [&](const std::pair<std::vector<bool>, std::vector<bool>> &pair) -> std::tuple<long long, double, double>
    {
        // Get unique subproblem ID for this task
        int mySubproblemId = subproblemCounter++;

        VLOG(1) << "Subproblem " << mySubproblemId << " starts" << endl;
    
        auto startTime = std::chrono::steady_clock::now();
        auto bm1 = pair.first;
        auto bm2 = pair.second;
        assert(bm1.size() == bm2.size());

        if(g_verbosity>=1){
            VLOG(1) << "Bit Mark pair :";
            for (auto const &bm : bm1)
            {
                cout << bm << " ";
            }
            cout << "--> ";
            for (auto const &bm : bm2)
            {
                cout << bm << " ";
            }
            cout << endl;
        }

        // Set counting configuration for this subproblem
        CountingConfig::setConfig(config.countingMethod, bm1, bm2, config.dumpAssignments, config.dumpFileName);
        vector<TreeNode *> prunedEnsemble1 = subProblemGen.pruneEnsemble(bm1, unaffectedTrees, affectedTrees);
        vector<TreeNode *> prunedEnsemble2 = subProblemGen.pruneEnsemble(bm2, unaffectedTrees, affectedTrees);
        assert(prunedEnsemble1.size() == affectedTrees.size());
        assert(prunedEnsemble2.size() == affectedTrees.size());
        Cudd manager;
        if (config.useDynamicOrdering) 
        {
            manager.AutodynEnable(CUDD_REORDER_SIFT);
            VLOG(2) << "Dynamic variable reordering enabled (SIFT)" << endl;
        }
        map<boolvar, ADD, BoolVarComparator> varMap;
        set<boolvar, BoolVarComparator> sbSplits;
        for (const auto &sp : splits)
        {
            if (config.sensitiveFeatures.count(sp.feature) == 0)
            {
                sbSplits.insert(sp);
            }
        }
        for (const auto &sp : sbSplits)
        {
            ADD add_var = manager.addVar();
            varMap[sp] = add_var;
        }
        assert(varMap.size() == sbSplits.size());

        // Create a local debug manager for this subproblem
        // Only enable debug output for the specified subproblem ID to save disk space
        int targetSubproblemId = (config.debugSubproblemId >= 0) ? config.debugSubproblemId : 0;
        bool enableDebugForThisSubproblem = config.enableDebugOutput && (mySubproblemId == targetSubproblemId);
        DebugOutputManager localDebugManager(enableDebugForThisSubproblem, "debug_output");
        SubproblemSolver solver(manager, precision, varMap, &localDebugManager);
        VLOG(2) << endl;
        set<boolvar, BoolVarComparator> varSet;
        for (auto it : varMap)
        {
            varSet.insert(it.first);
        }
        // Use pairwise subtraction instead of summing then subtracting
        ADD diffSum = solver.pairwise_subtract_and_sum(prunedEnsemble1, prunedEnsemble2, varSet, mySubproblemId);
        if (diffSum.IsZero())
        {
            VLOG(1) << "Subproblem " << mySubproblemId << " difference sum is zero." << endl;
            auto endTime = std::chrono::steady_clock::now();
            double tstart = std::chrono::duration<double, std::milli>(startTime.time_since_epoch()).count();
            double tend = std::chrono::duration<double, std::milli>(endTime.time_since_epoch()).count();
            assert(tend >= tstart);
            VLOG(1) << "Subproblem " << mySubproblemId << " ends" << endl;
            return std::make_tuple(0LL, tstart, tend);
        }

        VLOG(2) << "Gap value (before BDD) " << gap << endl;
        BDD diff = diffSum.BddThreshold(gap - unaffectedTrees.size() + 1);

        if (diff.IsZero())
        {
            VLOG(1) << "Subproblem " << mySubproblemId << " BDD is zero after applying gap threshold." << endl;
            VLOG(2) << "===============================" << endl;
            auto endTime = std::chrono::steady_clock::now();
            double tstart = std::chrono::duration<double, std::milli>(startTime.time_since_epoch()).count();
            double tend = std::chrono::duration<double, std::milli>(endTime.time_since_epoch()).count();
            assert(tend >= tstart);
            VLOG(1) << "Subproblem " << mySubproblemId << "ends" << endl;
            return std::make_tuple(0LL, tstart, tend);
        }

        // Apply dependency constraints to the BDD
        solver.applyConstraints(diff, varSet);

        VLOG(1) << "Processing Subproblem " << mySubproblemId << endl;
        long long subproblemCount = CountingWrapper::countSol(diff, manager, gap);

        auto endTime = std::chrono::steady_clock::now();
        double tstart = std::chrono::duration<double, std::milli>(startTime.time_since_epoch()).count();
        double tend = std::chrono::duration<double, std::milli>(endTime.time_since_epoch()).count();
        assert(tend >= tstart);

        VLOG(1) << "Subproblem " << mySubproblemId << " ends" << endl;
        return std::make_tuple(subproblemCount, tstart, tend);
    };

    // Initialize global Pepin state ONCE before processing any subproblems
    if (config.countingMethod == "pepin" || config.usePepinCounting)
    {
        PepinCounting::initGlobalPepin(config.pepinEps, config.pepinDelta, config.randomSeed, config.enableDebugOutput, config.enableSanityCheck);
    }
    for (auto &sp : spArray)
    {
        for (auto &pair : sp)
        {
            auto bm1 = pair.first;
            auto bm2 = pair.second;
            auto bmDiff = bitvecXor(bm1, bm2);

            //vector<bool> affectedTreeIndex;
            affectedTreeIndex.clear();
            for(const auto &tsv : treeSensVec)
            {
               affectedTreeIndex.push_back(bitvecOr(bitvecAnd(tsv, bmDiff)));
            }

            //vector<TreeNode *> unaffectedTrees;
            //vector<TreeNode *> affectedTrees;
            unaffectedTrees.clear();
            affectedTrees.clear();

            for (int i = 0; i < affectedTreeIndex.size(); i++)
            {
                if (affectedTreeIndex[i] == 1)
                {
                    affectedTrees.push_back(trees[i]);
                }
                else
                {
                    unaffectedTrees.push_back(trees[i]);
                }
            }
            VLOG(1) << "Unaffected Trees : " << unaffectedTrees.size() << endl;
            VLOG(1) << "Affected Trees : " << affectedTrees.size() << endl;
            SubProblemGenerator subProblemGen(affectedTrees, unaffectedTrees, sensSplits);
            auto res = solve_subproblem(pair);
            results.push_back(res);
        }
    }
    long long count = 0;
    int idx = 0;
    double maxSubproblemTimeMs = 0.0;

    for (const auto &res : results)
    {
        long long subproblemCount;
        double tstart, tend;
        std::tie(subproblemCount, tstart, tend) = res;
        double subproblemTimeMs = tend - tstart;
        VLOG(1) << "Subproblem " << idx++ << ": count = " << subproblemCount
                  << ", time taken = " << subproblemTimeMs << " s" << endl;
        count += subproblemCount;

        // Track maximum subproblem time
        if (subproblemTimeMs > maxSubproblemTimeMs)
        {
            maxSubproblemTimeMs = subproblemTimeMs;
        }
    }
    long long finalCount = CountingWrapper::getFinalCount(count, config.countingMethod);
    assert(finalCount >= 0);
    auto t2 = std::chrono::steady_clock::now();
    cout << "Total count (" << config.countingMethod << " counting): " << finalCount << endl;
    if (config.dumpAssignments)
    {
        string filename = config.dumpFileName;
        VLOG(2) << "[DEBUG] Dumping assignments to file: " << filename << endl;
        std::ofstream outfile(filename, std::ios::app);
        if (!outfile.is_open())
        {
            std::cerr << "Error opening file for writing: " << filename << std::endl;
            return 1;
        }
        outfile << "Precision: " << precision << endl;
        outfile << "Gap: " << gap << endl;
        outfile << "Sensitive features: ";
        for (int feature : config.sensitiveFeatures)
        {
            outfile << feature << " ";
        }

        outfile << endl;
        outfile << "No. of splits in the ensemble: " << splits.size() << endl;
        for (const auto &fg : featureGuardCount)
        {
            outfile << fg.first << " " << fg.second << endl;
        }
        outfile.close();
    }

    double wallTimeMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
    cout << "Total time taken: " << wallTimeMs << " ms" << endl;
    VLOG(1) << "Maximum subproblem time: " << maxSubproblemTimeMs << " ms" << endl;

    // Write log file
    logFile.open(logFileName, ios::app);
    if (logFile.is_open())
    {

        logFile << "Total final count (" << config.countingMethod << " counting): " << finalCount << "\n";
        logFile << "Total subproblem sum: " << count << "\n";
        int idx = 0;
        for (const auto &res : results)
        {
            long long subproblemCount;
            double tstart, tend;
            std::tie(subproblemCount, tstart, tend) = res;
            double subproblemTimeMs = tend - tstart;
            logFile << "Subproblem " << idx++ << ":\n"
                    << " count: " << subproblemCount << "\n"
                    << " time taken: " << subproblemTimeMs << " ms\n";
        }

        logFile << "Total time taken (wall): " << wallTimeMs << " ms\n";
        logFile << "Maximum subproblem time: " << maxSubproblemTimeMs << " ms\n";
        logFile.close();
        VLOG(1) << "Log written to: " << logFileName << endl;
    }
    else
    {
        cerr << "Failed to write log file: " << logFileName << endl;
    }
    double fraction_violation = finalCount;
    for (const auto &fg : featureGuardCount)
    {
        fraction_violation = fraction_violation / (fg.second + 1);
    }
    assert(std::isfinite(fraction_violation));
    VLOG(1) << "Fraction of solutions violating sensitive feature guards: " << fraction_violation << endl;
    return 0;
}
