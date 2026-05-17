#include "counting_boundary.hpp"
#include <atomic>

namespace CountingBoundary
{

    struct SolutionAssignment
    {
        std::string assignment;
        std::string M1;
        std::string M2;

        SolutionAssignment(const std::string &assign, std::string m1, std::string m2)
            : assignment(assign), M1(m1), M2(m2) {}

        // For use in std::set
        bool operator<(const SolutionAssignment &other) const
        {
            if (assignment != other.assignment)
                return assignment < other.assignment;
            if (M1 != other.M1)
                return M1 < other.M1;
            return M2 < other.M2;
        }
    };

    set<SolutionAssignment> globalAssignmentsSet;

    // Helper function to enumerate all satisfying assignments and store SolutionAssignment objects
    void enumerateSatisfyingAssignments(const BDD &bdd, Cudd &manager,
                                        const std::vector<bool> &bitmask1,
                                        const std::vector<bool> &bitmask2)
    {
        if (bdd.IsZero())
            return;

        int numVars = manager.ReadSize();
        DdGen *gen;
        int *cube;
        CUDD_VALUE_TYPE value;

        Cudd_ForeachCube(manager.getManager(), bdd.getNode(), gen, cube, value)
        {
            // Extract assignment from cube representation
            std::vector<bool> assignmentVector(numVars, false);
            std::string assignStr = "";
            for (int i = 0; i < numVars; i++)
            {
                if (cube[i] == 1)
                {
                    assignmentVector[i] = true;
                    assignStr += "1";
                }
                else if (cube[i] == 0)
                {
                    assignmentVector[i] = false;
                    assignStr += "0";
                }
                else // don't care
                {
                    assignmentVector[i] = false;
                    assignStr += "-";
                }
            }

            // Find all don't care positions
            std::vector<int> dontCarePositions;
            for (int i = 0; i < numVars; i++)
            {
                if (cube[i] == 2)
                {
                    dontCarePositions.push_back(i);
                }
            }
            int numDontCares = dontCarePositions.size();
            int totalAssignments = 1 << numDontCares;

            for (int assignment = 0; assignment < totalAssignments; assignment++)
            {
                std::vector<bool> specificAssignment = assignmentVector;
                for (int j = 0; j < numDontCares; j++)
                {
                    int pos = dontCarePositions[j];
                    bool bitValue = (assignment >> j) & 1;
                    specificAssignment[pos] = bitValue;
                }
                // Build assignment string for this specific assignment
                std::string assignStrSpecific = "";
                for (int i = 0; i < numVars; i++)
                {
                    assignStrSpecific += (specificAssignment[i] ? "1" : "0");
                }
                // Compute M1 and M2 (for now, just use assignStrSpecific as both)
                std::string m1 = "";
                std::string m2 = "";
                for (int i = 0; i < bitmask1.size(); i++)
                {
                    m1 += (bitmask1[i] ? "1" : "0");
                    m2 += (bitmask2[i] ? "1" : "0");
                }
                globalAssignmentsSet.insert(SolutionAssignment(assignStrSpecific, m1, m2));
            }
        }
    }

    void DumpAssignmentsToFile(const BDD &bdd, Cudd &manager,
                               const std::vector<bool> &bitmask1, const std::vector<bool> &bitmask2,
                               bool dump_assignments, const std::string &filename)
    {
        // Silence intentionally unused parameters in this overload
        (void)bdd;
        (void)manager;
        (void)bitmask1;
        (void)bitmask2;
        (void)dump_assignments;

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
        outfile << "# Total unique assignments: " << globalAssignmentsSet.size() << std::endl;
        for (const auto &entry : globalAssignmentsSet)
        {
            outfile << entry.assignment << " " << entry.M1 << " " << entry.M2 << std::endl;
        }
        outfile.close();
        std::cout << "Dumped " << globalAssignmentsSet.size() << " entries to file: " << filename << std::endl;
    }

    long long countSolutions(BDD &bdd, Cudd &manager, double gap,
                             const std::vector<bool> &bitmask1, const std::vector<bool> &bitmask2,
                             bool dump_assignments, const std::string &dump_file)
    {
        (void)gap; // Currently unused in boundary counting

        double satSol = 0;


        BDD gapBDD = bdd;


        satSol = gapBDD.CountMinterm(manager.ReadSize());
        if (dump_assignments)
        {
            enumerateSatisfyingAssignments(gapBDD, manager, bitmask1, bitmask2);
            DumpAssignmentsToFile(gapBDD, manager, bitmask1, bitmask2, dump_assignments, dump_file);
        }
        return static_cast<long long>(satSol);
    }
 
}