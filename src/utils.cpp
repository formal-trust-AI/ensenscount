#include "utils.hpp"
#include <cmath>
#include <sstream>

// Global verbosity variable
int g_verbosity = 1; // default to normal

void printBitMask(const vector<bool> &mask)
{
    for (bool bit : mask)
    {
        cout << (bit ? 1 : 0) << " ";
    }
    cout << endl;
}

void parseDT(TreeNode *node)
{
    if (!node)
    {
        return;
    }
    else if (!(node->is_leaf))
    {
        cout << endl;
        cout << "Node details :" << endl;
        cout << "Node ID: " << node->nodeid << endl;
        cout << node->feature << " " << node->split_condition << endl;
        cout << node->is_leaf << endl;
        cout << node->treeid << endl;
    }
    else
    {
        cout << "Leaf Value :" << node->leaf << endl;
        return;
    }

    parseDT(node->yes);
    parseDT(node->no);
}
// Helper function to calculate BDD width using CountMinterm * 2
double calculateBDDWidth(const BDD &bdd, Cudd &manager)
{   
    // Use CountMinterm * 2 to get the number of satisfying assignments
    double minterm_count = bdd.CountMinterm(manager.ReadSize());
    return minterm_count * 2;
}

TreeNode *parseTreeNodeOriginal(const json &node, int treeCount)
{
    if (!node.contains("nodeid"))
    {
        cerr << "Error: Missing 'nodeid' in JSON node." << endl;
        return nullptr;
    }

    if (node.contains("leaf"))
    {
        double leafValue = node["leaf"].get<double>();
        return new TreeNode(node["nodeid"].get<int>(), leafValue);
    }
    else
    {
        if (!node.contains("split") || !node.contains("split_condition") ||
            !node.contains("yes") || !node.contains("no"))
        {
            cerr << "Error: Missing required fields in non-leaf node." << endl;
            return nullptr;
        }

        TreeNode *treeNode = new TreeNode(
            node["nodeid"].get<int>(),
            node["split"].get<int>(),
            node["split_condition"].get<double>(),
            nullptr,
            nullptr);

        if (node.contains("children") && node["children"].is_array())
        {
            for (const auto &child : node["children"])
            {
                TreeNode *childNode = parseTreeNodeOriginal(child, treeCount);
                if (!childNode)
                {
                    delete treeNode;
                    return nullptr;
                }
                if (childNode->nodeid == node["yes"].get<int>())
                {
                    treeNode->yes = childNode;
                }
                else if (childNode->nodeid == node["no"].get<int>())
                {
                    treeNode->no = childNode;
                }
            }
        }
        treeNode->treeid = treeCount;
        return treeNode;
    }
}

TreeNode *parseTreeNode(const json &node, int treeCount, int precision)
{
    if (!node.contains("nodeid"))
    {
        cerr << "Error: Missing 'nodeid' in JSON node." << endl;
        return nullptr;
    }

    if (node.contains("leaf"))
    {
        double leafValue = node["leaf"].get<double>();

        if (precision > 0)
        {
            // Scale by 10^precision and round to nearest int, then store as double
            double scaled = leafValue * pow(10, precision);
            double roundedValue = static_cast<double>(static_cast<int>(round(scaled)));
            return new TreeNode(node["nodeid"].get<int>(), roundedValue);
        }
        else
        {
            // For precision -1 or <= 0, store the original double value
            return new TreeNode(node["nodeid"].get<int>(), leafValue);
        }
    }
    else
    {
        if (!node.contains("split") || !node.contains("split_condition") ||
            !node.contains("yes") || !node.contains("no"))
        {
            cerr << "Error: Missing required fields in non-leaf node." << endl;
            return nullptr;
        }

        TreeNode *treeNode = new TreeNode(
            node["nodeid"].get<int>(),
            node["split"].get<int>(),
            node["split_condition"].get<double>(),
            nullptr,
            nullptr);

        if (node.contains("children") && node["children"].is_array())
        {
            for (const auto &child : node["children"])
            {
                TreeNode *childNode = parseTreeNode(child, treeCount, precision);
                if (!childNode)
                {
                    delete treeNode;
                    return nullptr;
                }
                if (childNode->nodeid == node["yes"].get<int>())
                {
                    treeNode->yes = childNode;
                }
                else if (childNode->nodeid == node["no"].get<int>())
                {
                    treeNode->no = childNode;
                }
            }
        }
        treeNode->treeid = treeCount;
        return treeNode;
    }
}

TreeNode *parseTreeNodeUpDown(const json &node, int treeCount, int precision)
{

    if (!node.contains("nodeid"))
    {
        cerr << "Error: Missing 'nodeid' in JSON node." << endl;
        return nullptr;
    }

    if (node.contains("leaf"))
    {
        double leafValue = node["leaf"].get<double>();

        if (precision > 0)
        {
            // Scale by 10^precision and round to nearest int, then store as double
            // cout<<"Leaf Value (before scaling) "<<leafValue<<endl;
            double scaled = leafValue * pow(10, precision);
            // cout<<"Leaf Value (after scaling) "<<scaled<<endl;
            double roundedValue = static_cast<double>(static_cast<int>(round(scaled)));
            int upRoundedValue = static_cast<int>(ceil(scaled));
            int downRoundedValue = static_cast<int>(floor(scaled));
            // cout<<upRoundedValue<<endl;
            // cout << leafValue << " " << upRoundedValue << " " << downRoundedValue << endl;
            return new TreeNode(node["nodeid"].get<int>(), roundedValue, upRoundedValue, downRoundedValue);
        }
        else
        {
            // For precision -1 or <= 0, store the original double value
            return new TreeNode(node["nodeid"].get<int>(), leafValue);
        }
    }
    else
    {
        if (!node.contains("split") || !node.contains("split_condition") ||
            !node.contains("yes") || !node.contains("no"))
        {
            cerr << "Error: Missing required fields in non-leaf node." << endl;
            return nullptr;
        }

        TreeNode *treeNode = new TreeNode(
            node["nodeid"].get<int>(),
            node["split"].get<int>(),
            node["split_condition"].get<double>(),
            nullptr,
            nullptr);

        if (node.contains("children") && node["children"].is_array())
        {
            for (const auto &child : node["children"])
            {
                TreeNode *childNode = parseTreeNodeUpDown(child, treeCount, precision);
                if (!childNode)
                {
                    delete treeNode;
                    return nullptr;
                }
                if (childNode->nodeid == node["yes"].get<int>())
                {
                    treeNode->yes = childNode;
                }
                else if (childNode->nodeid == node["no"].get<int>())
                {
                    treeNode->no = childNode;
                }
            }
        }
        treeNode->treeid = treeCount;
        return treeNode;
    }
}

int executeTree(TreeNode *node, const bool upSide,
                const map<int, map<double, bool>> &guard_assignments)
{
    // if(node->is_leaf) return node->upLeaf;
    if (node->is_leaf)
    {
        if (upSide)
            return node->upLeaf;
        else
            return node->downLeaf;
    }
    auto f = node->feature;
    auto c = node->split_condition;
    auto truth_value = guard_assignments.at(f).at(c);
    if (truth_value)
    {
        return executeTree(node->yes, upSide, guard_assignments);
    }
    else
    {
        return executeTree(node->no, upSide, guard_assignments);
    }
}

int executeEnsemble(vector<TreeNode *> &ensemble, const bool upSide,
                    const map<int, map<double, bool>> &guard_assignments)
{
    int result = 0;
    for (auto t : ensemble)
    {
        auto tmp = executeTree(t, upSide, guard_assignments);
        cout << tmp << "\n";
        result += tmp;
    }
    return result;
}

void read_guard_assignments(string fname,
                            map<int, map<double, bool>> &guard_assignments)
{
    ifstream file(fname);
    if (!file.is_open())
    {
        throw runtime_error("Could not open file");
    }
    string line;
    while (getline(file, line))
    {
        if (line.empty())
            continue;
        stringstream ss(line);
        string token;

        int key1;
        double key2;
        bool value;

        // Read int
        getline(ss, token, ',');
        key1 = stoi(token);

        // Read double
        getline(ss, token, ',');
        key2 = stod(token);

        // Read bool
        getline(ss, token, ',');
        if (token == "true" || token == "1")
            value = true;
        else
            value = false;
        // cout << key1 << key2 << value;
        guard_assignments[key1][key2] = value;
    }
}

int executeEnsemble(vector<TreeNode *> &ensemble, const bool upSide,
                    string fname)
{
    map<int, map<double, bool>> guard_assignments;
    read_guard_assignments(fname, guard_assignments);
    auto result = executeEnsemble(ensemble, upSide, guard_assignments);
    cout << "Result of the run on assignment in " << fname << ":" << result << endl;
    return result;
}

void executeEnsemble(vector<TreeNode *> &ensemble, string f1, string f2)
{
    auto r1 = executeEnsemble(ensemble, false, f1);
    auto r2 = executeEnsemble(ensemble, true, f2);
    cout << "Final swing:" << r1 - r2;
}

ProgramConfig parseCommandLine(int argc, char *argv[])
{

    // Set defaults
    ProgramConfig config;
    config.precision = -1;
    config.gap = 0.1;
    config.useDynamicOrdering = false;
    config.bitDistance = 1;                  // Default bit distance
    config.localConcurrency = 1;             // Default: process 1 subproblem at a time from this instance
    config.enableDebugOutput = false;        // Default: no debug output
    config.debugSubproblemId = -1;           // Default: debug first subproblem (ID 0)
    config.enableSanityCheck = false;        // Default: no sanity checking
    config.dumpAssignments = false;          // Default: do not dump assignments
    config.dumpFileName = "assignments.txt"; // Default dump file name
    // New scheduling defaults
    config.timeoutSeconds = -1;         // No timeout by default
    config.memoryLimitMB = -1;          // No memory limit by default
    config.maxConcurrentJobs = 2;       // Default to 2 concurrent jobs globally
    config.countingMethod = "naive"; // Default to naive counting
    config.jsonFilePath2 = "";

    // Pepin algorithm defaults
    config.usePepinCounting = false; // Default: use exact counting
    config.pepinEps = 0.1;           // Default error parameter
    config.pepinDelta = 0.1;         // Default confidence parameter
    config.randomSeed = 0;           // Default: use random_device
    config.splitsFilePath = "";   // Default: no splits file
    // Secondary guard defaults
    config.splitGap = 0.0; // Default: disabled (0.0 means no Secondary guards)

    // Verbosity default
    config.verbosity = 1; // normal

    bool gapProvided = false;
    bool sensitiveProvided = false;

    // Define the option structure for getopt_long
    static struct option longOptions[] = {
        {"file", required_argument, 0, 'f'},
        {"file2", no_argument, 0, 'F'},
        {"precision", required_argument, 0, 'p'},
        {"gap", required_argument, 0, 'g'},
        {"approx", required_argument, 0, 'a'},
        {"dynamic", no_argument, 0, 'd'},
        {"sensitive", required_argument, 0, 's'},
        {"debug", no_argument, 0, 'D'},
        {"debug-subproblem", required_argument, 0, 'S'},
        {"sanity-check", no_argument, 0, 'X'},
        {"timeout", required_argument, 0, 't'},
        {"memory-limit", required_argument, 0, 'm'},
        {"max-jobs", required_argument, 0, 'c'},
        {"counting-method", required_argument, 0, 'M'},
        {"pepin", no_argument, 0, 'A'},
        {"pepin-eps", required_argument, 0, 'E'},
        {"pepin-delta", required_argument, 0, 'L'},
        {"seed", required_argument, 0, 'R'},
        {"bit-distance", required_argument, 0, 'k'},
        {"max-procs", required_argument, 0, 'j'},
        {"log-file", required_argument, 0, 'l'},
        {"dump-assignments", required_argument, 0, 'O'},
        {"split-gap", required_argument, 0, 'v'},
        {"additional-file", required_argument, 0, 'F'},
        {"verbosity", required_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},

        {0, 0, 0, 0}};

    int optionIndex = 0;
    int opt;

    while ((opt = getopt_long(argc, argv, "f:p:g:a:ds:hDk:j:t:m:c:S:M:AE:L:R:XO:l:v:F:B:V:", longOptions, &optionIndex)) != -1)
    {
        switch (opt)
        {
        case 'l':
            config.logFileName = optarg;
            break;
        case 'f':
            config.jsonFilePath = optarg;
            break;
        case 'F':
            config.jsonFilePath2 = optarg;
            break;
        case 'p':
            config.precision = atoi(optarg);
            break;
        case 'g':
            config.gap = atof(optarg);
            gapProvided = true;
            break;
        case 'a':
            config.approx2 = atoi(optarg);
            break;
        case 'd':
            config.useDynamicOrdering = true;
            break;
        case 's':
            config.sensitiveFeatures.insert(atoi(optarg));
            sensitiveProvided = true;
            break;
        case 'D':
            config.enableDebugOutput = true;
            break;
        case 'S':
            config.debugSubproblemId = atoi(optarg);
            break;
        case 'X':
            config.enableSanityCheck = true;
            break;
        case 'h':
            printUsage(argv[0]);
            exit(0);
            break;
        case 'k':
            config.bitDistance = atoi(optarg);
            break;
        case 'j':
            config.localConcurrency = atoi(optarg);
            break;
        case 't':
            config.timeoutSeconds = atoi(optarg);
            break;
        case 'm':
            config.memoryLimitMB = atoi(optarg);
            break;
        case 'c':
            config.maxConcurrentJobs = atoi(optarg);
            break;
        case 'M':
            config.countingMethod = optarg;
            if (config.countingMethod != "boundary" && config.countingMethod != "naive" && config.countingMethod != "pepin")
            {
                cerr << "Error: counting-method must be 'boundary', 'naive', or 'pepin'" << endl;
                printUsage(argv[0]);
                exit(1);
            }
            if (config.countingMethod == "pepin")
            {
                config.usePepinCounting = true;
            }
            break;
        case 'A':
            config.usePepinCounting = true;
            config.countingMethod = "pepin";
            break;
        case 'E':
            config.pepinEps = atof(optarg);
            break;
        case 'L':
            config.pepinDelta = atof(optarg);
            break;
        case 'R':
            config.randomSeed = atoi(optarg);
            break;
        case 'O':
            config.dumpAssignments = true;
            if (optarg == nullptr)
            {
                std::cerr << "Error: -O flag requires a filename argument." << std::endl;
                printUsage(argv[0]);
                exit(1);
            }
            config.dumpFileName = optarg;
            break;
        case 'v':
            config.splitGap = atof(optarg);
            break;
        case 'B':
            config.splitsFilePath = optarg;
            break;
        case 'V':
            config.verbosity = atoi(optarg);
            break;
        default:
            printUsage(argv[0]);
            exit(1);
        }

    }
    // Check if required arguments are provided
    if (config.jsonFilePath.empty())
    {
        cerr << "Error: JSON file path is required." << endl;
        printUsage(argv[0]);
        exit(1);
    }

    // Enforce mandatory -g and -s
    if (!gapProvided)
    {
        cerr << "Error: -g (gap) is a required argument." << endl;
        printUsage(argv[0]);
        exit(1);
    }
    if (!sensitiveProvided)
    {
        cerr << "Error: -s (at least one sensitive feature) is a required argument." << endl;
        printUsage(argv[0]);
        exit(1);
    }

    // Set global verbosity variable from config
    g_verbosity = config.verbosity;

    return config;
}
double reverse_sigmoid(double y)
{
    // To avoid division by zero or log of zero
    const double eps = 1e-15; // small epsilon for numerical stability

    if (y <= 0.0)
        y = eps;
    if (y >= 1.0)
        y = 1.0 - eps;

    return std::log(y / (1.0 - y));
}

void printUsage(const char *programName)
{
    cout << "Usage: " << programName << " [OPTIONS]" << endl;
    cout << "Options:" << endl;
    cout << "  -f, --file PATH           Path to JSON file (required)" << endl;
    cout << "  -p, --precision N         Precision for approximation (default: 2, negative for exact)" << endl;
    cout << "  -g, --gap VALUE           Gap threshold (default: 0.1)" << endl;
    cout << "  -a, --approx N            Use every Nth variable (default: 2)" << endl;
    cout << "  -d, --dynamic             Use dynamic variable ordering (default: custom ordering)" << endl;
    cout << "  -s, --sensitive FEATURE   Sensitive feature (can be used multiple times)" << endl;
    cout << "  -D, --debug               Enable debug output (trees and ADDs)" << endl;
    cout << "  -S, --debug-subproblem N  Which subproblem to debug (default: 0, use with -D)" << endl;
    cout << "  -X, --sanity-check        Enable sanity checking of assignments" << endl;
    cout << "  -h, --help                Display this help message" << endl;
    cout << "  -k, --bit-distance VALUE  Bit distance (default: 1)" << endl;
    cout << "  -j, --max-procs VALUE     Maximum concurrent subproblems from this instance (default: 1)" << endl;
    cout << "  -t, --timeout SECONDS     Timeout for individual subproblems in seconds (-1 for no timeout)" << endl;
    cout << "  -m, --memory-limit MB     Memory limit for individual subproblems in MB (-1 for no limit)" << endl;
    cout << "  -c, --max-jobs N          Maximum concurrent subproblems across ALL instances (default: 2)" << endl;
    cout << "  -M, --counting-method TYPE Counting method: 'boundary' (default), 'naive', or 'pepin'" << endl;
    cout << "  -A, --pepin               Use Pepin approximation algorithm instead of exact counting" << endl;
    cout << "  -E, --pepin-eps EPSILON   Error parameter for Pepin algorithm (default: 0.1)" << endl;
    cout << "  -L, --pepin-delta DELTA   Confidence parameter for Pepin algorithm (default: 0.1)" << endl;
    cout << "  -O, --dump-assignments    Dump satisfying assignments to file" << endl;
    cout << "  -R, --seed SEED           Random seed for reproducibility (0 = use random_device)" << endl;
    cout << "  -v, --split-gap VALUE     Split gap for Secondary guards on non-sensitive features (default: 0.0, disabled)" << endl;
    cout << "  -F, --additional-file PATH  Path to an additional file" << endl;
    cout << "  -B, --splits-file PATH   Path to splits file" << endl;
    cout << "  -V, --verbosity LEVEL    Verbosity level (default: 1)" << endl;
}

std::map<int, std::pair<double, double>> getFeatureRanges(
    const std::set<boolvar, BoolVarComparator> &splits)
{
    std::map<int, std::pair<double, double>> featureRanges;

    // Iterate through all splits to find min and max for each feature
    for (const auto &split : splits)
    {
        if (featureRanges.find(split.feature) == featureRanges.end())
        {
            // First occurrence of this feature
            featureRanges[split.feature] = {split.split_val, split.split_val};
        }
        else
        {
            // Update min and max
            featureRanges[split.feature].first = std::min(featureRanges[split.feature].first, split.split_val);
            featureRanges[split.feature].second = std::max(featureRanges[split.feature].second, split.split_val);
        }
    }

    return featureRanges;
}

void addSecondaryGuards(
    std::set<boolvar, BoolVarComparator> &splits,
    double splitGap,
    const std::set<int> &sensitiveFeatures,
    int precision, std::set<boolvar, BoolVarComparator> &ogSplits)
{
    // If splitGap is 0 or negative, this feature is disabled
    if (splitGap <= 0.0)
    {
        return;
    }

    // Get the current ranges for all features
    auto featureRanges = getFeatureRanges(splits);

    // For each feature that is not sensitive
    for (const auto &featureRange : featureRanges)
    {
        int feature = featureRange.first;

        // Skip sensitive features
        if (sensitiveFeatures.find(feature) != sensitiveFeatures.end())
        {
            continue;
        }
        // add a check that diff between any split should be greater than splitGap
        std::vector<double> existingSplits;
        for (const auto &split : splits)
        {
            if (split.feature == feature)
            {
                existingSplits.push_back(split.split_val);
            }
        }
        // in the case when provided gap is greater than existing splits gap warn do not give error, go forward with using the min diff value for that feature

        std::sort(existingSplits.begin(), existingSplits.end());
        // find the min gap
        double minExistingGap = std::numeric_limits<double>::max();
        for (size_t i = 1; i < existingSplits.size(); ++i)
        {
            double gap = existingSplits[i] - existingSplits[i - 1];
            if (gap < minExistingGap)
            {
                minExistingGap = gap;   
            }
        }
        double usableSplitGap = splitGap;
        if (minExistingGap < splitGap){
            std::cout << "# Warning: Provided split gap " << splitGap << " is greater than existing min gap " << minExistingGap << " for feature " << feature << ". Using existing min gap instead." << std::endl;
            usableSplitGap = minExistingGap;
        }

        double minVal = featureRange.second.first;
        double maxVal = featureRange.second.second;

        // Generate Secondary guards at uniform intervals
        // Start from minVal and go up to maxVal with the given gap
        // remove ogSplits from splits
        for (const auto &split : ogSplits)
        {
            if (split.feature == feature)
            {
                splits.erase(split);
            }
        }
        
        for (double guardVal = minVal; guardVal <= maxVal + usableSplitGap / 2.0; guardVal += usableSplitGap)
        {
            // Round to avoid floating point precision issues
            guardVal = std::round(guardVal * 1e10) / 1e10;

            // Only add if not already in splits
            boolvar guard(feature, guardVal);
            if (splits.find(guard) == splits.end())
            {
                splits.insert(guard);
            }
        }

    }

    // std::cout << "# Secondary guards added for split gap: " << splitGap << std::endl;
}

/**
 * Helper function to recursively update tree nodes with nearest guard values
 */
void replaceTreeNodeSplits(
    TreeNode *node,
    const std::map<int, std::vector<double>> &guardsByFeature,
    const std::set<int> &sensitiveFeatures)
{
    if (!node || node->is_leaf)
    {
        return;
    }

    int feature = node->feature;

    // Skip sensitive features
    if (sensitiveFeatures.find(feature) != sensitiveFeatures.end())
    {
        // Still recurse into children for sensitive features
        replaceTreeNodeSplits(node->yes, guardsByFeature, sensitiveFeatures);
        replaceTreeNodeSplits(node->no, guardsByFeature, sensitiveFeatures);
        return;
    }

    // For non-sensitive features, find the nearest guard value
    if (guardsByFeature.find(feature) != guardsByFeature.end())
    {
        const auto &guards = guardsByFeature.at(feature);

        // Find the nearest guard to the current split value
        double nearestGuard = guards[0];
        double minDistance = std::abs(node->split_condition - guards[0]);

        for (double guard : guards)
        {
            double distance = std::abs(node->split_condition - guard);
            if (distance < minDistance)
            {
                minDistance = distance;
                nearestGuard = guard;
            }
        }

        // Replace with nearest guard value
        if (node->split_condition != nearestGuard)
        {
            node->split_condition = nearestGuard;
        }
    }

    // Recurse into children
    replaceTreeNodeSplits(node->yes, guardsByFeature, sensitiveFeatures);
    replaceTreeNodeSplits(node->no, guardsByFeature, sensitiveFeatures);
}

/**
 * Replace non-sensitive feature splits with the nearest Secondary guard values
 * This updates both the splits set and all tree nodes in the ensemble
 * @param splits The set of splits to modify
 * @param trees Vector of tree ensembles to update
 * @param sensitiveFeatures Set of sensitive feature IDs (which will NOT be replaced)
 */
void replaceWithNearestGuards(
    std::set<boolvar, BoolVarComparator> &splits,
    std::vector<TreeNode *> &trees,
    const std::set<int> &sensitiveFeatures)
{
    // Build a map of guards for each non-sensitive feature from og to nearest secondary split
    std::map<int, std::vector<double>> guardsByFeature; // feature -> list
    for (const auto &split : splits)
    {
        int feature = split.feature;

        // Skip sensitive features
        if (sensitiveFeatures.find(feature) != sensitiveFeatures.end())
        {
            continue;
        }

        guardsByFeature[feature].push_back(split.split_val);
    }
    // Sort guard values for each feature for efficient nearest neighbor search
    for (auto &entry : guardsByFeature)
    {
        std::sort(entry.second.begin(), entry.second.end());
    }
  

    // Update all tree nodes
    for (TreeNode *tree : trees)
    {
        replaceTreeNodeSplits(tree, guardsByFeature, sensitiveFeatures);
    }

    std::cout << "# Non-sensitive feature splits replaced with nearest guards" << std::endl;
}

class Assignment
{
public:
    map<int, string> feature_to_bitmask;
    string getString()
    {
        string result;
        for (const auto &pair : feature_to_bitmask)
        {
            result += pair.second;
        }
        return result;
    }
    void setFeatureBitmask(int feature, const string &bitmask)
    {
        feature_to_bitmask[feature] = bitmask;
    }
    void setFeatureBitmask(int feature, const vector<bool> &bitmask)
    {
        string bm;
        for (bool b : bitmask)
        {
            bm += b ? '1' : '0';
        }
        feature_to_bitmask[feature] = bm;
    }
    void print()
    {
        for (const auto &pair : feature_to_bitmask)
        {
            cout << "Feature " << pair.first << ": " << pair.second << endl;
        }
    }
};

void printTree(TreeNode *node, int depth)
{
    if (!node)
        return;

    // Indentation for better visualization
    for (int i = 0; i < depth; ++i)
        cout << "  ";

    if (node->is_leaf)
    {
        cout << "Leaf (Node ID: " << node->nodeid << ", Value: " << node->leaf << ")\n";
    }
    else
    {
        cout << "Node ID: " << node->nodeid << ", Feature: " << node->feature
             << ", Split: " << node->split_condition << "\n";
        printTree(node->yes, depth + 1);
        printTree(node->no, depth + 1);
    }
}


/*
splits file format:
Feature 0 <= -0.810376525
Feature 0 <= -0.737583876
Feature 0 <= -0.664791107
Feature 4 <= -0.615021586
Feature 4 <= -0.225193799
Feature 4 <= 0.16463396
Feature 4 <= 0.554461718
Feature 4 <= 0.944289505
Feature 5 <= -1.40701854
Feature 5 <= -0.0790933371
Feature 7 <= -0.590046406
Feature 7 <= 1.90388989
Feature 10 <= 0.486081272
Feature 10 <= 0.533881664
Feature 10 <= 0.537014961
Feature 10 <= 0.82515049
Feature 10 <= 0.877084255
Feature 11 <= 4.20847464
Feature 11 <= 4.23578262
Feature 11 <= 5.2908659
Feature 12 <= 0.0886925235

Unique splits found: 21


*/

void collectSplitsFromFile(
    const std::string &filePath,
    std::set<boolvar, BoolVarComparator> &splits)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: Unable to open the splits file: " << filePath << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string featureStr, leqStr;
        int feature;
        double splitVal;

        if (!(iss >> featureStr >> feature >> leqStr >> splitVal))
        {
            std::cerr << "Error: Invalid line format in splits file: " << line << std::endl;
            continue;
        }

        boolvar split(feature, splitVal);
        splits.insert(split);
    }

}
// Function to parse the second model file and extract non-sensitive splits
// std::vector<TreeNode *> parseSecondModelFile(const std::string &filePath)
// {
//     std::ifstream file(filePath);
//     if (!file.is_open())
//     {
//         std::cerr << "Error: Unable to open the second model file: " << filePath << std::endl;
//         return {};
//     }

//     json modelJson;
//     file >> modelJson;

//     std::vector<TreeNode *> secondModelTrees;
//     int treeCount = 0;

//     if (!modelJson.contains("trees") || !modelJson["trees"].is_array())
//     {
//         std::cerr << "Error: The JSON structure is invalid or 'trees' is not an array." << std::endl;
//         return {};
//     }

//     for (const auto &tree : modelJson["trees"])
//     {
//         TreeNode *root = parseTreeNode(tree, treeCount, -1); // Precision -1 for original values
//         if (root)
//         {
//             secondModelTrees.push_back(root);
//         }
//         treeCount++;
//     }

//     return secondModelTrees;
// }
