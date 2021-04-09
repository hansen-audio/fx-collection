// Copyright(c) 2021 Hansen Audio.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ha {
namespace fx_collection {
//------------------------------------------------------------------------
using i32            = std::int32_t const;
using mut_i32        = std::int32_t;
using real           = float const;
using mut_real       = float;
using audio_sample_t = mut_real;

constexpr std::size_t NUM_CHANNELS          = 4;
constexpr std::size_t BYTE_ALIGNMENT        = 16;
using audio_frame_t alignas(BYTE_ALIGNMENT) = std::array<audio_sample_t, NUM_CHANNELS>;

constexpr audio_frame_t zero_audio_frame{0., 0., 0., 0.};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha