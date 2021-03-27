// Copyright(c) 2021 Hansen Audio.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace ha {
namespace fx_collection {
//------------------------------------------------------------------------
using i32            = std::int32_t;
using real_t         = float;
using audio_sample_t = real_t;

/**
 * audio frame container
 */
constexpr std::size_t ARRAY_SIZE     = 4;
constexpr std::size_t BYTE_ALIGNMENT = 16;
// Pack 4 samples into a frame, 16-byte aligned
using audio_frame_t alignas(BYTE_ALIGNMENT) = std::array<audio_sample_t, ARRAY_SIZE>;
constexpr audio_frame_t zero_audio_frame{0., 0., 0., 0.};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha