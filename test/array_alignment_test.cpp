// Copyright(c) 2021 Hansen Audio.

#include "ha/fx_collection/types.h"

#include "gtest/gtest.h"

using namespace ha::fx_collection;

namespace {

//-----------------------------------------------------------------------------
TEST(array_alignment_test, test_16_byte_alignment)
{
    EXPECT_EQ(std::alignment_of<audio_frame>::value, 16);
}

//-----------------------------------------------------------------------------
} // namespace
