#include "counting_naive.hpp"
#include <atomic>
#include <sstream>
#include <iostream>
#include <mutex>
#include <cassert>
#include <cudd.h> // For CUDD's low-level API

namespace CountingNaive
{
    // Global hashset for naive counting - stores individual M1x and M2x values
    // This single set ensures NO OVERCOUNTING of individual values across all subproblems
    std::unordered_set<std::string> globalSolutionSet;
    std::multimap<std::string, pair<std::string, std::string>> assignmentsMap; // Maps x to (m1, m2) pairs
    // Mutex for thread-safe access to global set
    std::mutex globalSetMutex;
    // Global debug flag (set by interface, like in pepin_counting.cpp)
    bool debug_enabled = true;

    // Function to enable debug output at runtime
    void setDebugEnabled(bool enabled)
    {
        debug_enabled = enabled;
    }

    // Function to clear the global solution set
    void clearGlobalSet()
    {
        std::lock_guard<std::mutex> lock(globalSetMutex);
        globalSolutionSet.clear();
    }

    // Function to get the size of the global solution set
    size_t getGlobalSetSize()
    {
        std::lock_guard<std::mutex> lock(globalSetMutex);
        return globalSolutionSet.size();
    }

    // Function to get all elements from the global solution set
    std::vector<std::string> getGlobalSetElements()
    {
        std::lock_guard<std::mutex> lock(globalSetMutex);
        return std::vector<std::string>(globalSolutionSet.begin(), globalSolutionSet.end());
    }

    // Global variables for cube enumeration
    static long long g_newSolutionsCount = 0;
    static long long g_assignmentCount = 0;
    void dumpGlobalSetToFile(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(globalSetMutex);
        std::cerr << "[DEBUG] dumpGlobalSetToFile called with filename: '" << filename << "'" << std::endl;
        if (filename.empty())
        {
            std::cerr << "Error: dump file name is empty. Skipping assignment dump." << std::endl;
            return;
        }
        std::ofstream outfile(filename);
        if (!outfile.is_open())
        {
            std::cerr << "Error opening file for writing: " << filename << std::endl;
            return;
        }
        outfile << "# Total unique assignments: " << assignmentsMap.size() << std::endl;
        size_t entry_idx = 0;
        for (const auto &entry : assignmentsMap)
        {
            const std::string &x = entry.first;
            const std::string &m1 = entry.second.first;
            const std::string &m2 = entry.second.second;

            if (x.empty() || m1.empty() || m2.empty())
            {
                std::cerr << "[DEBUG] Warning: empty value in assignmentsMap at index " << entry_idx << std::endl;
            }
            outfile << x << " " << m1 << " " << m2 << std::endl;
            ++entry_idx;
        }
        outfile.close();
        VLOG(1) << "Dumped " << assignmentsMap.size() << " entries to file: " << filename << std::endl;
    }
    // Helper function to enumerate all satisfying assignments using Cudd_ForeachCube
    void enumerateSatisfyingAssignments(BDD bdd, Cudd &manager,
                                        const std::vector<bool> &bitmask1,
                                        const std::vector<bool> &bitmask2,
                                        long long &newSolutionsCount)
    {
        assert(manager.getManager() != nullptr);
        assert(bdd.getNode() != nullptr);
        assert(bitmask1.size() == bitmask2.size());
        if (bdd.IsZero())
        {
            return;
        }

        // Get the number of variables
        int numVars = manager.ReadSize();
        assert(numVars >= 0);

        // Debug: Show bitmask content
        VLOG(1) << "Bitmask1 (" << bitmask1.size() << " bits): ";
        for (size_t i = 0; i < bitmask1.size(); i++)
        {
            VLOG(1) << (bitmask1[i] ? "1" : "0");
        }
        VLOG(1) << std::endl;

        double minterms = bdd.CountMinterm(numVars);
        assert(std::isfinite(minterms));
        VLOG(1) << "MinterCount: " << minterms << std::endl;
        VLOG(1) << "NumVars: " << numVars << std::endl;

        // Set up global variables for enumeration
        g_newSolutionsCount = 0;
        g_assignmentCount = 0;


        DdGen *gen;
        int *cube;
        CUDD_VALUE_TYPE value;

        // Start cube enumeration
        std::map<string, string> assgn_to_cube;
        Cudd_ForeachCube(manager.getManager(), bdd.getNode(), gen, cube, value)
        {
            g_assignmentCount++;
            assert(cube != nullptr);

            // Extract assignment from cube representation
            std::vector<bool> assignmentVector(numVars, false);
            std::string cubeStr = "";

            for (int i = 0; i < numVars; i++)
            {
                assert(cube[i] == 0 || cube[i] == 1 || cube[i] == 2);
                if (cube[i] == 1)
                {
                    assignmentVector[i] = true;
                    cubeStr += "1";
                }
                else if (cube[i] == 0)
                {
                    assignmentVector[i] = false;
                    cubeStr += "0";
                }
                else // cube[i] == 2 means don't care
                {
                    assignmentVector[i] = false; // Default to 0 for don't care
                    cubeStr += "-";
                }
            }

            // Find all don't care positions
            std::vector<int> dontCarePositions;
            for (int i = 0; i < numVars; i++)
            {
                if (cube[i] == 2) // Don't care
                {
                    dontCarePositions.push_back(i);
                }
            }

            // Generate all possible assignments for don't care variables
            int numDontCares = dontCarePositions.size();
            assert(numDontCares >= 0);
            assert(numDontCares < static_cast<int>(sizeof(int) * 8));
            int totalAssignments = 1 << numDontCares; // 2^numDontCares

            for (int assignment = 0; assignment < totalAssignments; assignment++)
            {
                // Create a specific assignment for this iteration
                std::vector<bool> specificAssignment = assignmentVector;

                // Set don't care variables according to current assignment number
                for (int j = 0; j < numDontCares; j++)
                {
                    int pos = dontCarePositions[j];
                    assert(pos >= 0 && pos < numVars);
                    bool bitValue = (assignment >> j) & 1;
                    specificAssignment[pos] = bitValue;
                }

                // Generate M1x and M2x from this specific assignment
                std::string x = "";
                for (int i = 0; i < numVars; i++)
                {

                    x += (specificAssignment[i] ? "1" : "0");
                }
                std::string m1 = "";
                std::string m2 = "";
                for (int i = 0; i < bitmask1.size(); i++) // Append bitmask values for insensitive features
                {
                    assert(bitmask2.size() == bitmask1.size());
                    m1 += (bitmask1[i] ? "1" : "0");
                    m2 += (bitmask2[i] ? "1" : "0");
                }
                std::string m1x = x + m1;
                std::string m2x = x + m2;
                {
                    std::lock_guard<std::mutex> lock(globalSetMutex);
                    assert(m1x.size() == static_cast<size_t>(numVars) + bitmask1.size());
                    assert(m2x.size() == static_cast<size_t>(numVars) + bitmask2.size());

                    bool m1Added = globalSolutionSet.insert(m1x).second;
                    bool m2Added = globalSolutionSet.insert(m2x).second;
                    if (assgn_to_cube.find(m1x) == assgn_to_cube.end())
                    {
                        assgn_to_cube[m1x] = cubeStr;
                    }
                    else
                    {
                        if (assgn_to_cube[m1x] != cubeStr)
                        {
                            std::cerr << "[WARNING] Different cubes map to the same M1x: " << m1x << std::endl;
                            std::cerr << "          Existing cube: " << assgn_to_cube[m1x] << std::endl;
                            std::cerr << "          New cube:      " << cubeStr << std::endl;
                        }
                    }
                    if (assgn_to_cube.find(m2x) == assgn_to_cube.end())
                    {
                        assgn_to_cube[m2x] = cubeStr;
                    }
                    else
                    {
                        if (assgn_to_cube[m2x] != cubeStr)
                        {
                            std::cerr << "[WARNING] Different cubes map to the same M2x: "
                                      << m2x << std::endl;
                            std::cerr << "          Existing cube: " << assgn_to_cube[m2x] << std::endl;
                            std::cerr << "          New cube:      " << cubeStr << std::endl;
                        }
                    }
                    // cout << m1x << endl
                    //      << m2x << endl;
                    if (m1Added)
                    {
                        g_newSolutionsCount++;
                        assignmentsMap.insert({x, {m1, m2}});
                    }
                    if (m2Added)
                    {
                        g_newSolutionsCount++;
                        assignmentsMap.insert({x, {m2, m1}});
                    }
                }
            }
        }


        // Update the output parameter
        newSolutionsCount = g_newSolutionsCount;
        assert(newSolutionsCount == g_newSolutionsCount);
        assert(newSolutionsCount >= 0);

    }

    long long countSolutions(BDD &bdd, Cudd &manager, double gap,
                             const std::vector<bool> &bitmask1, const std::vector<bool> &bitmask2, bool dump_assignments, const std::string &dump_file)
    {
        assert(manager.getManager() != nullptr);
        assert(bdd.getNode() != nullptr);
        assert(bitmask1.size() == bitmask2.size());
        assert(std::isfinite(gap));
        VLOG(1) << "=== Naive Counting: ADD/BDD Analysis ===" << std::endl;
        VLOG(1) << "Input BDD IsZero: " << (bdd.IsZero() ? "true" : "false") << std::endl;
        VLOG(1) << "Input BDD Node Count: " << bdd.nodeCount() << std::endl;
        VLOG(1) << "Gap threshold: " << gap << std::endl;

        // Convert ADD to BDD using threshold
        BDD gapBDD = bdd;

        VLOG(1) << "  BDD IsZero: " << (gapBDD.IsZero() ? "true" : "false") << std::endl;
        VLOG(1) << "  BDD Node Count: " << gapBDD.nodeCount() << std::endl;

        if (!gapBDD.IsZero())
        {
            double minterms = gapBDD.CountMinterm(manager.ReadSize());
            assert(std::isfinite(minterms));
            VLOG(1) << "  BDD CountMinterm: " << minterms << std::endl;
        }

        if (gapBDD.IsZero())
        {
            cout << "Gap BDD is zero after thresholding. No solutions to count." << endl;
            return 0;
        }

        // Enumerate all satisfying assignments and store M1x, M2x in global set
        long long newSolutionsCount = 0;
        enumerateSatisfyingAssignments(gapBDD, manager, bitmask1, bitmask2, newSolutionsCount);

        if (dump_assignments)
        {
            //cout << "Dumping assignments to file: " << dump_file << endl;
            dumpGlobalSetToFile(dump_file);
        }
        if (newSolutionsCount > 0)
        {
            VLOG(1) << "Added " << newSolutionsCount << " new unique M1x and M2x values to global set" << std::endl;
        }
        // Return number of new unique values added to the global set
        // cout << "[DEBUG] GlobalSolutionSet size after enumeration: " << getGlobalSetSize() << std::endl;
        assert(newSolutionsCount >= 0);
        return newSolutionsCount;
    }
}
