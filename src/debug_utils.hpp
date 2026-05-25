#ifndef DEBUG_UTILS_HPP
#define DEBUG_UTILS_HPP

#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <atomic>
#include <filesystem>
#include <system_error>
#include "utils.hpp"
#include "tree_exporter.hpp"
#include "cuddObj.hh"

class DebugOutputManager
{
private:
    bool enabled;
    std::string baseDirectory;
    std::atomic<int> currentSubproblem; // Make it atomic for thread safety

    void createDirectory(const std::string &path) const
    {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            std::cerr << "Warning: failed to create directory '" << path
                      << "': " << ec.message() << std::endl;
        }
    }

public:
    DebugOutputManager(bool enable = false, const std::string &baseDir = "debug_output")
        : enabled(enable), baseDirectory(baseDir), currentSubproblem(0)
    {
        if (enabled)
        {
            createDirectory(baseDirectory);
        }
    }

    void setEnabled(bool enable)
    {
        enabled = enable;
    }

    bool isEnabled() const
    {
        return enabled;
    }

    void exportOriginalEnsemble(const std::vector<TreeNode *> &trees)
    {
        if (!enabled)
            return;

        std::string ensembleDir = baseDirectory + "/original_ensemble";
        createDirectory(ensembleDir);

        for (size_t i = 0; i < trees.size(); ++i)
        {
            std::string filename = ensembleDir + "/tree_" + std::to_string(i) + ".dot";
            DecisionTreeExporter::saveAsDot(trees[i], filename);
        }

        std::cout << "Exported original ensemble (" << trees.size() << " trees) to: " << ensembleDir << std::endl;
    }

    void exportScaledEnsemble(const std::vector<TreeNode *> &trees)
    {
        if (!enabled)
            return;

        std::string ensembleDir = baseDirectory + "/scaled_ensemble";
        createDirectory(ensembleDir);

        for (size_t i = 0; i < trees.size(); ++i)
        {
            std::string filename = ensembleDir + "/tree_" + std::to_string(i) + ".dot";
            DecisionTreeExporter::saveAsDot(trees[i], filename);
        }

        std::cout << "Exported scaled ensemble (" << trees.size() << " trees) to: " << ensembleDir << std::endl;
    }

    void exportSubproblemData(const std::vector<TreeNode *> &ensemble1,
                              const std::vector<TreeNode *> &ensemble2,
                              const ADD &add1,
                              const ADD &add2,
                              Cudd &manager)
    {
        if (!enabled)
            return;

        // Get a unique subproblem ID atomically
        int subproblemId = currentSubproblem.fetch_add(1);
        exportSubproblemDataForId(ensemble1, ensemble2, add1, add2, manager, subproblemId);
    }

    void exportSubproblemDataForId(const std::vector<TreeNode *> &ensemble1,
                                   const std::vector<TreeNode *> &ensemble2,
                                   const ADD &add1,
                                   const ADD &add2,
                                   Cudd &manager,
                                   int subproblemId)
    {
        if (!enabled)
            return;

        std::string subproblemDir = baseDirectory + "/subproblem_" + std::to_string(subproblemId);
        createDirectory(subproblemDir);

        // Export pruned ensembles
        exportPrunedEnsembles(ensemble1, ensemble2, subproblemDir);

        // Export ADDs
        exportADDs(add1, add2, manager, subproblemDir);
    }

    void exportIntermediateADD(const ADD &intermediateADD,
                               Cudd &manager,
                               int treeIndex,
                               int ensembleId,
                               const std::string &ensembleName,
                               int forceSubproblemId = -1) // Allow explicit subproblem ID
    {
        if (!enabled)
            return;

    // Explicitly silence unused parameter warning if not used
    (void)ensembleId;

    // Use provided subproblem ID or get current one
        int subproblemId = (forceSubproblemId >= 0) ? forceSubproblemId : currentSubproblem.load();
        std::string subproblemDir = baseDirectory + "/subproblem_" + std::to_string(subproblemId);
        createDirectory(subproblemDir);

        // Create intermediate ADD directory
        std::string intermediateDir = subproblemDir + "/intermediate_adds";
        createDirectory(intermediateDir);

        exportSingleADD(intermediateADD, manager,
                        intermediateDir + "/" + ensembleName + "_after_tree_" + std::to_string(treeIndex) + ".dot");
    }

private:
    void exportPrunedEnsembles(const std::vector<TreeNode *> &ensemble1,
                               const std::vector<TreeNode *> &ensemble2,
                               const std::string &subproblemDir)
    {

        // Export ensemble 1
        for (size_t i = 0; i < ensemble1.size(); ++i)
        {
            std::string filename = subproblemDir + "/ensemble1_tree_" + std::to_string(i) + ".dot";
            DecisionTreeExporter::saveAsDot(ensemble1[i], filename);
        }

        // Export ensemble 2
        for (size_t i = 0; i < ensemble2.size(); ++i)
        {
            std::string filename = subproblemDir + "/ensemble2_tree_" + std::to_string(i) + ".dot";
            DecisionTreeExporter::saveAsDot(ensemble2[i], filename);
        }
    }

    void exportADDs(const ADD &add1, const ADD &add2, Cudd &manager, const std::string &subproblemDir)
    {
        int numVars = Cudd_ReadSize(manager.getManager());
        std::vector<std::string> varNames;
        std::vector<const char *> inames;

        for (int i = 0; i < numVars; i++)
        {
            varNames.push_back("x" + std::to_string(i));
        }

        for (const auto &name : varNames)
        {
            inames.push_back(name.c_str());
        }

        // Export ADD1
        {
            std::string add1_file = subproblemDir + "/ADD1.dot";
            FILE *fp1 = fopen(add1_file.c_str(), "w");
            if (fp1)
            {
                DdNode *node1 = add1.getNode();
                Cudd_DumpDot(manager.getManager(), 1, &node1, inames.data(), nullptr, fp1);
                fclose(fp1);
            }
            else
            {
                std::cerr << "Error opening " << add1_file << std::endl;
            }
        }

        // Export ADD2
        {
            std::string add2_file = subproblemDir + "/ADD2.dot";
            FILE *fp2 = fopen(add2_file.c_str(), "w");
            if (fp2)
            {
                DdNode *node2 = add2.getNode();
                Cudd_DumpDot(manager.getManager(), 1, &node2, inames.data(), nullptr, fp2);
                fclose(fp2);
            }
            else
            {
                std::cerr << "Error opening " << add2_file << std::endl;
            }
        }
    }

    void exportSingleADD(const ADD &add, Cudd &manager, const std::string &filename)
    {
        int numVars = Cudd_ReadSize(manager.getManager());
        std::vector<std::string> varNames;
        std::vector<const char *> inames;

        for (int i = 0; i < numVars; i++)
        {
            varNames.push_back("x" + std::to_string(i));
        }

        for (const auto &name : varNames)
        {
            inames.push_back(name.c_str());
        }

        FILE *fp = fopen(filename.c_str(), "w");
        if (fp)
        {
            DdNode *node = add.getNode();
            Cudd_DumpDot(manager.getManager(), 1, &node, inames.data(), nullptr, fp);
            fclose(fp);
        }
        else
        {
            std::cerr << "Error opening " << filename << std::endl;
        }
    }
};

#endif // DEBUG_UTILS_HPP
