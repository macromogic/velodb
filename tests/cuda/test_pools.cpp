#include "../common/test_warmup_utility.hpp"
#include "cuda/event.hpp"
#include "cuda/event_pool.hpp"
#include "cuda/stream.hpp"
#include "cuda/stream_pool.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class PoolTest : public test::VeloDBTest {
protected:
    void SetUp() override { test::VeloDBTest::SetUp(); }
};

TEST_F(PoolTest, StreamPoolBasic)
{
    StreamPool pool(2, 4);

    EXPECT_EQ(pool.availableCount(), 2);
    EXPECT_EQ(pool.totalCount(), 2);

    {
        auto stream1 = pool.acquire();
        ASSERT_TRUE(stream1);
        EXPECT_TRUE(stream1.value().isValid());
        EXPECT_EQ(pool.availableCount(), 1);

        auto stream2 = pool.acquire();
        ASSERT_TRUE(stream2);
        EXPECT_EQ(pool.availableCount(), 0);

        // Should expand
        auto stream3 = pool.acquire();
        ASSERT_TRUE(stream3);
        EXPECT_EQ(pool.totalCount(), 3);
    }
    // Streams should be returned to pool here
    EXPECT_EQ(pool.availableCount(), 3);
}

TEST_F(PoolTest, EventPoolBasic)
{
    EventPool pool(2, 4);

    {
        auto event1 = pool.acquire();
        ASSERT_TRUE(event1);
        EXPECT_TRUE(event1.value().isValid());

        auto event2 = pool.acquire();
        ASSERT_TRUE(event2);
    }

    auto event1_again = pool.acquire();
    ASSERT_TRUE(event1_again);
}

TEST_F(PoolTest, StreamPoolExhaustion)
{
    StreamPool pool(1, 1); // Max size 1

    auto stream1 = pool.acquire();
    ASSERT_TRUE(stream1);

    auto stream2 = pool.acquire();
    // Should fail as max size is reached and pool is empty
    ASSERT_FALSE(stream2);
}

TEST_F(PoolTest, EventPoolExhaustion)
{
    EventPool pool(1, 1); // Max size 1

    auto event1 = pool.acquire();
    ASSERT_TRUE(event1);

    auto event2 = pool.acquire();
    ASSERT_FALSE(event2);
}
