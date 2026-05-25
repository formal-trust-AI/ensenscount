#include "utils.hpp"
#include <cmath> // For pow and round functions

// Helper functions
void collectSplits(TreeNode *node, set<boolvar, BoolVarComparator> &splits)
{
    if (!node->is_leaf)
    {
        splits.insert(boolvar(node->feature, node->split_condition));
        collectSplits(node->yes, splits);
        collectSplits(node->no, splits);
    }
}

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
            //cout<<"Leaf Value (before scaling) "<<leafValue<<endl;
            double scaled = leafValue * pow(10, precision);
            //cout<<"Leaf Value (after scaling) "<<scaled<<endl;
            double roundedValue = static_cast<double>(static_cast<int>(round(scaled)));
            int upRoundedValue = static_cast<int>(ceil(scaled));
            int downRoundedValue = static_cast<int>(floor(scaled));
            // cout<<upRoundedValue<<endl;
            cout << leafValue << " " << upRoundedValue << " "<< downRoundedValue<<endl;
            return new TreeNode(node["nodeid"].get<int>(), roundedValue, upRoundedValue,downRoundedValue);
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

ProgramConfig parseCommandLine(int argc, char *argv[])
{

    // Set defaults
    ProgramConfig config;
    config.precision = -1;
    config.gap = 0.1;
    config.approx2 = 1;
    config.useDynamicOrdering = false;
    config.bitDistance = 1; // Default bit distance
    config.max_procs = 30;  // Default max processes

    // Define the option structure for getopt_long
    static struct option longOptions[] = {
        {"file", required_argument, 0, 'f'},
        {"precision", required_argument, 0, 'p'},
        {"gap", required_argument, 0, 'g'},
        {"approx", required_argument, 0, 'a'},
        {"dynamic", no_argument, 0, 'd'},
        {"sensitive", required_argument, 0, 's'},
        {"bit-distance", required_argument, 0, 'k'},
        {"num-cores", required_argument, 0, 'j'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    int optionIndex = 0;
    int opt;

    while ((opt = getopt_long(argc, argv, "f:p:g:a:ds:h:k:j:l:", longOptions, &optionIndex)) != -1)
    {
        switch (opt)
        {
        case 'l':
            config.logFileName = optarg;
            break;
        case 'f':
            config.jsonFilePath = optarg;
            break;
        case 'p':
            config.precision = atoi(optarg);
            break;
        case 'g':
            config.gap = atof(optarg);
            break;
        case 'a':
            config.approx2 = atoi(optarg);
            break;
        case 'd':
            config.useDynamicOrdering = true;
            break;
        case 's':
            config.sensitiveFeatures.insert(atoi(optarg));
            break;
        case 'h':
            printUsage(argv[0]);
            exit(0);
            break;
        case 'k':
            config.bitDistance = atoi(optarg);
            break;
        case 'j':
            config.max_procs = atoi(optarg);
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

    return config;
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
    cout << "  -h, --help                Display this help message" << endl;
    cout << "  -k, --bit-distance VALUE  Bit distance (default: 1)" << endl;
    cout << "  -j, --num_cores           number of threads(default 1)" << endl;
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
