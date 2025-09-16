#pragma once

#include "common/profiler.hpp"
#include "cuda/warmup.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace velodb {
namespace test {

    class CudaTestWarmup {
    public:
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

        static bool isAvailable() { return initializeOnce(); }
    };

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
