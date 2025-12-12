#include "../common/test_warmup_utility.hpp"
#include "data/bit_vector.hpp"

#include <gtest/gtest.h>

using namespace velodb;

class BitVectorTest : public test::VeloDBTest {
protected:
    void SetUp() override { test::VeloDBTest::SetUp(); }
};

TEST_F(BitVectorTest, BasicOperations)
{
    BitVector bv(100);

    bv.set(10);
    bv.set(50);

    EXPECT_TRUE(bv.get(10));
    EXPECT_TRUE(bv.get(50));
    EXPECT_FALSE(bv.get(0));
    EXPECT_FALSE(bv.get(99));

    bv.unset(10);
    EXPECT_FALSE(bv.get(10));
}

TEST_F(BitVectorTest, Resize)
{
    BitVector bv(10);
    bv.set(5);

    bv.resize(20);
    EXPECT_TRUE(bv.get(5));
    EXPECT_FALSE(bv.get(15));

    bv.set(15);
    EXPECT_TRUE(bv.get(15));
}

TEST_F(BitVectorTest, Slice)
{
    BitVector bv(10);
    bv.set(2);
    bv.set(5);
    bv.set(8);

    // Slice from 2 to 7 (exclusive) -> indices 2, 3, 4, 5, 6
    // New indices: 0->2, 1->3, 2->4, 3->5, 4->6
    BitVector sliced = bv.slice(2, 7);

    EXPECT_TRUE(sliced.get(0)); // Old 2
    EXPECT_FALSE(sliced.get(1)); // Old 3
    EXPECT_FALSE(sliced.get(2)); // Old 4
    EXPECT_TRUE(sliced.get(3)); // Old 5
    EXPECT_FALSE(sliced.get(4)); // Old 6
}

TEST_F(BitVectorTest, DataLocation)
{
    BitVector bv(100);
    bv.set(10);

    bv.to(DataLocation::CUDA);
    // We can't easily check values on device without copying back or using a kernel
    // But we can check if it crashes

    bv.to(DataLocation::HOST);
    EXPECT_TRUE(bv.get(10));
}
