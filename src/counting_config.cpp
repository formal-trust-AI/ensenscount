#include "counting_config.hpp"

namespace CountingConfig
{

    // Global configuration variables
    std::string currentCountingMethod = "boundary";
    std::vector<bool> currentBitmask1;
    std::vector<bool> currentBitmask2;
    PepinConfig currentPepinConfig;
    bool dump_assignments;
    std::string dump_file;

    void setConfig(const std::string &countingMethod,
                   const std::vector<bool> &bitmask1,
                   const std::vector<bool> &bitmask2,
                   bool dump_assign,
                   const std::string &dump_filename)
    {
        currentCountingMethod = countingMethod;
        currentBitmask1 = bitmask1;
        currentBitmask2 = bitmask2;
        dump_assignments = dump_assign;
        dump_file = dump_filename;
    }

    void setPepinConfig(double eps, double delta, unsigned int seed)
    {
        currentPepinConfig.eps = eps;
        currentPepinConfig.delta = delta;
        currentPepinConfig.seed = seed;
    }

    std::string getCountingMethod()
    {
        return currentCountingMethod;
    }

    std::pair<std::vector<bool>, std::vector<bool>> getBitmasks()
    {
        return std::make_pair(currentBitmask1, currentBitmask2);
    }

    PepinConfig getPepinConfig()
    {
        return currentPepinConfig;
    }
}