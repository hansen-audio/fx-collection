// Copyright(c) 2021 Hansen Audio.

#include "detail/shuffle_note.h"
#include "ha/fx_collection/trance_gate.h"

#include "gtest/gtest.h"

using namespace ha::fx_collection;

namespace {

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_setting_mix)
{
    auto cx = trance_gate::create();
    trance_gate::set_sample_rate(cx, real(44100.));
    trance_gate::set_mix(cx, 1.);
    EXPECT_EQ(cx.mix, 1.);
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_delay_active)
{
    auto cx = trance_gate::create();
    trance_gate::set_sample_rate(cx, real(44100.));
    EXPECT_FALSE(cx.is_delay_active);
    trance_gate::trigger(cx, real(1. / 32.));
    EXPECT_TRUE(cx.is_delay_active);
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_fade_in_active)
{
    auto cx = trance_gate::create();
    trance_gate::set_sample_rate(cx, real(44100.));
    EXPECT_FALSE(cx.is_fade_in_active);
    trance_gate::trigger(cx, real(0.), real(1. / 32.));
    EXPECT_TRUE(cx.is_fade_in_active);
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_processing)
{
    constexpr i32 LEFT_CH = 0;
    auto cx               = trance_gate::create();
    trance_gate::set_sample_rate(cx, real(44100.));
    trance_gate::set_step_count(cx, 8);
    trance_gate::set_step(cx, LEFT_CH, 0, real(1.));
    trance_gate::set_step(cx, LEFT_CH, 1, real(0.));
    trance_gate::set_step(cx, LEFT_CH, 2, real(1.));
    trance_gate::set_step(cx, LEFT_CH, 3, real(0.));
    trance_gate::set_step(cx, LEFT_CH, 4, real(1.));
    trance_gate::set_step(cx, LEFT_CH, 5, real(1.));
    trance_gate::set_step(cx, LEFT_CH, 6, real(1.));
    trance_gate::set_step(cx, LEFT_CH, 7, real(0.));
    trance_gate::set_step_len(cx, real(1. / 32.));

    mut_i32 numSamples = 1024;
    audio_frame sum{real(0.), real(0.), real(0.), real(0.)};
    while (numSamples-- > 0)
    {
        audio_frame in{real(0.5), real(0.5), real(0.), real(0.)};
        audio_frame out{real(0.), real(0.), real(0.), real(0.)};
        trance_gate::process(cx, in, out);
        for (mut_i32 i = 0; i < sum.data.size(); ++i)
            sum.data[i] += out.data[i];
    }

    EXPECT_TRUE(sum.data[0] > real(468.));
    EXPECT_TRUE(sum.data[1] > real(468.));
    EXPECT_TRUE(sum.data[0] < real(469.));
    EXPECT_TRUE(sum.data[1] < real(469.));
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_is_shuffle_note_16)
{
    mut_i32 step_index      = 0;
    constexpr real note_len = real(1. / 16.);

    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index, note_len));
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_is_shuffle_note_32)
{
    mut_i32 step_index      = 0;
    constexpr real note_len = real(1. / 32.);

    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_FALSE(detail::is_shuffle_note(step_index++, note_len));
    EXPECT_TRUE(detail::is_shuffle_note(step_index++, note_len));
}

//-----------------------------------------------------------------------------
} // namespace
