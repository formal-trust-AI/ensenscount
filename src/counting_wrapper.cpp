#include "counting_wrapper.hpp"
#include "counting_boundary.hpp"
#include "counting_naive.hpp"
#include "counting_config.hpp"
#include "pepin_counting.hpp"
#include <iostream>
#include <stdexcept>

namespace CountingWrapper
{

    long long countSol(BDD &bdd, Cudd &manager, double gap)
    {
        // Use global configuration for method and bitmasks
        return countSol(bdd, manager, gap, "", {}, {});
    }

    long long countSol(BDD &bdd, Cudd &manager, double gap,
                       const std::string &countingMethod,
                       const std::vector<bool> &bitmask1,
                       const std::vector<bool> &bitmask2
                    )
    {
        // Use provided method, or fall back to global config
        std::string method = countingMethod.empty() ? CountingConfig::getCountingMethod() : countingMethod;
        auto bitmasks = CountingConfig::getBitmasks();
        std::vector<bool> mask1 = bitmask1.empty() ? bitmasks.first : bitmask1;
        std::vector<bool> mask2 = bitmask2.empty() ? bitmasks.second : bitmask2;
        bool dump_assignments = CountingConfig::dump_assignments;
        std::string dump_file = CountingConfig::dump_file;
        
        if (method == "boundary")
        {
            return CountingBoundary::countSolutions(bdd, manager, gap, mask1, mask2,dump_assignments, dump_file);
        }
        else if (method == "naive")
        {
            return CountingNaive::countSolutions(bdd, manager, gap, mask1, mask2, dump_assignments, dump_file);
        }
        else if (method == "pepin")
        {
            // For Pepin, we get parameters from global config
            auto config = CountingConfig::getPepinConfig();
            return PepinCounting::countSolutions(bdd, manager, gap, mask1, mask2,
                                                 config.eps, config.delta, config.seed);
        }
        else
        {
            throw std::invalid_argument("Unknown counting method: " + method);
        }
    }

    long long getFinalCount(long long subproblemSum, const std::string &countingMethod)
    {
        std::string method = countingMethod.empty() ? CountingConfig::getCountingMethod() : countingMethod;

        if (method == "boundary")
        {
            // For boundary counting, the final count is just the sum of all subproblem counts
            return subproblemSum;
        }
        else if (method == "naive")
        {
            // For naive counting, the final count is the size of the global set
            return static_cast<long long>(CountingNaive::getGlobalSetSize());
        }
        else if (method == "pepin")
        {
            // For Pepin counting, return the scaled estimate (samples / probability)
            long long globalCount = PepinCounting::getFinalGlobalCount();
            PepinCounting::clearGlobalPepin();
            return globalCount;
        }
        else
        {
            throw std::invalid_argument("Unknown counting method: " + method);
        }
    }

    void displayGlobalSetElements()
    {
        std::string method = CountingConfig::getCountingMethod();
        if (method == "naive")
        {
            // Display the global set elements
            auto elements = CountingNaive::getGlobalSetElements();
            std::cout << "Global set contains " << elements.size() << " unique elements:" << std::endl;
            for (const auto &element : elements)
            {
                std::cout << "  " << element << std::endl;
            }
        }
        else
        {
            std::cout << "Global set display is only available for naive counting method." << std::endl;
        }
    }
}