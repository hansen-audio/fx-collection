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

using f32     = float const;
using mut_f32 = std::remove_const<f32>::type;

using f64     = double const;
using mut_f64 = std::remove_const<f64>::type;

using audio_sample = mut_real;

constexpr std::size_t NUM_CHANNELS   = 4;
constexpr std::size_t BYTE_ALIGNMENT = 16;
struct AudioFrame
{
    alignas(BYTE_ALIGNMENT) std::array<audio_sample, NUM_CHANNELS> data;
};

constexpr AudioFrame zero_audio_frame{0., 0., 0., 0.};

//------------------------------------------------------------------------
} // namespace ha::fx_collection
