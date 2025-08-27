#pragma once

#include "common/profiler.hpp"
#include "cuda/warmup.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace velodb {
namespace test {

    /**
     * @brief CUDA Test Warmup Utility
     *
     * This class ensures CUDA is properly warmed up before running tests
     * to avoid the 4.6-second initialization delay on first table creation.
     */
    class CudaTestWarmup {
    public:
        /**
         * @brief Initialize CUDA for tests (call once per test executable)
         * @return true if warmup succeeded, false otherwise
         */
        static bool initializeOnce()
        {
            static bool initialized = false;
            static bool success = false;

            if (!initialized) {
                PROFILE_SCOPE("CUDA Test Warmup");
                auto result = runtime_warmup();
                success = static_cast<bool>(result);
                if (!success) {
                    std::cerr << "CUDA warmup failed: " << result.error() << std::endl;
                } else {
                    std::cout << "CUDA warmup completed successfully" << std::endl;
                }
                initialized = true;
            }

            return success;
        }

        /**
         * @brief Check if CUDA warmup is available (don't fail if CUDA not available)
         * @return true if CUDA is available and warmed up
         */
        static bool isAvailable() { return initializeOnce(); }
    };

    /**
     * @brief Base test class that automatically handles CUDA warmup
     *
     * Inherit from this class instead of ::testing::Test to get automatic
     * CUDA warmup without performance penalty.
     */
    class VeloDBTest : public ::testing::Test {
    protected:
        static void SetUpTestSuite()
        {
            // Warm up CUDA once per test suite
            CudaTestWarmup::initializeOnce();
        }

        void SetUp() override
        {
            // Reset profiler before each test
            Profiler::getInstance().reset();
        }

        void TearDown() override
        {
            // Optional: Print profile report after each test
            // Uncomment if you want detailed profiling for each test
            Profiler::getInstance().printReport();
        }
    };

} // namespace test
} // namespace velodb
