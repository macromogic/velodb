#pragma once

#include "common/profiler.hpp"
#include "cuda/task_manager.hpp"
#include "cuda/warmup.hpp"
#include "execution/execution_engine.hpp"

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

    class VelODBTest : public ::testing::Test {
    public:
        VelODBTest()
            : catalog_()
            , task_manager_()
            , engine_(catalog_, task_manager_)
        {
            VELODB_ASSERT_MSG(task_manager_.start(), "Failed to start TaskManager in test setup");
        }

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

            // Sync and clear oblivious manager BEFORE stopping task manager
            // to avoid use-after-free when ObliviousTableManager::~ObliviousTableManager
            // tries to wait for shuffles
            if (catalog_.hasObliviousManager()) {
                catalog_.getObliviousManager().syncAllShuffles();
            }

            task_manager_.stop(0);
        }

        Catalog catalog_;
        TaskManager task_manager_;
        ExecutionEngine engine_;
    };

} // namespace test
} // namespace velodb
