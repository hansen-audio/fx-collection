// Copyright(c) 2021 Hansen Audio.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ha {
namespace fx_collection {
//------------------------------------------------------------------------
using i32     = std::int32_t;
using float_t = float;

constexpr std::size_t ARRAY_SIZE           = 4;
constexpr std::size_t BYTE_ALIGNMENT       = 16;
using vector_float alignas(BYTE_ALIGNMENT) = std::array<float_t, ARRAY_SIZE>;
constexpr vector_float ZERO_ARRAY{0., 0., 0., 0.};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha