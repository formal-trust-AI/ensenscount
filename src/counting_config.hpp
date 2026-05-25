#ifndef COUNTING_CONFIG_HPP
#define COUNTING_CONFIG_HPP

#include <string>
#include <vector>

/**
 * Global counting configuration
 * This allows the counting wrapper to access the counting method and bitmasks
 * without passing them through every function call.
 */

namespace CountingConfig
{

    // Pepin configuration structure
    struct PepinConfig
    {
        double eps = 0.1;
        double delta = 0.1;
        unsigned int seed = 0;
    };

    // Global configuration variables
    extern std::string currentCountingMethod;
    extern std::vector<bool> currentBitmask1;
    extern std::vector<bool> currentBitmask2;
    extern PepinConfig currentPepinConfig;
    extern bool dump_assignments;
    extern std::string dump_file;

    /**
     * Set the counting configuration for the current subproblem
     * @param countingMethod The counting method to use ("boundary", "naive", "pepin", etc.)
     * @param bitmask1 Bitmask for the first ensemble (M1)
     * @param bitmask2 Bitmask for the second ensemble (M2)
     */
    void setConfig(const std::string &countingMethod,
                   const std::vector<bool> &bitmask1 = {},
                   const std::vector<bool> &bitmask2 = {},
                   bool dump_assign = false,
                   const std::string &dump_filename = "assignments.txt");

    /**
     * Set the Pepin algorithm configuration
     * @param eps Error parameter for Pepin algorithm
     * @param delta Confidence parameter for Pepin algorithm
     * @param seed Random seed for reproducibility
     */
    void setPepinConfig(double eps, double delta, unsigned int seed);

    /**
     * Get the current counting method
     * @return Current counting method
     */
    std::string getCountingMethod();

    /**
     * Get the current bitmasks
     * @return Pair of current bitmasks (M1, M2)
     */
    std::pair<std::vector<bool>, std::vector<bool>> getBitmasks();

    /**
     * Get the current Pepin configuration
     * @return Current Pepin configuration
     */
    PepinConfig getPepinConfig();
}

#endif // COUNTING_CONFIG_HPP