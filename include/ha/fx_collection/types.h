// Copyright(c) 2021 Hansen Audio.

#pragma once

#include "ha/aligned_allocator/allocator.h"
#include <cstdint>
#include <vector>

namespace ha {
namespace fx_collection {
//------------------------------------------------------------------------
constexpr std::size_t VECTOR_SIZE    = 4;
constexpr std::size_t BYTE_ALIGNMENT = 16;
using i32                            = std::int32_t;
using float_t                        = float;
using vector_float = std::vector<float_t, alignment::aligned_allocator<float_t, BYTE_ALIGNMENT>>;

static const vector_float ZERO_VECTOR(VECTOR_SIZE, float_t(0.));

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha