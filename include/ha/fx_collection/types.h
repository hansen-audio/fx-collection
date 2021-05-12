// Copyright(c) 2021 Hansen Audio.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ha::fx_collection {
//------------------------------------------------------------------------
using i32     = std::int32_t const;
using mut_i32 = std::remove_const<i32>::type;

using real     = float const;
using mut_real = std::remove_const<real>::type;

using audio_sample = mut_real;

constexpr std::size_t NUM_CHANNELS   = 4;
constexpr std::size_t BYTE_ALIGNMENT = 16;
struct audio_frame
{
    alignas(BYTE_ALIGNMENT) std::array<audio_sample, NUM_CHANNELS> data;
};

constexpr audio_frame zero_audio_frame{0., 0., 0., 0.};

//------------------------------------------------------------------------
} // namespace ha::fx_collection
