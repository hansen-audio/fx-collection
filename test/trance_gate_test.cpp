// Copyright(c) 2021 Hansen Audio.

#include "detail/shuffle_note.h"
#include "ha/fx_collection/trance_gate.h"

#include "gtest/gtest.h"

using namespace ha::fx_collection;

namespace {

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_setting_mix)
{
    auto trance_gate = TranceGateImpl::create();
    TranceGateImpl::set_sample_rate(trance_gate, real(44100.));
    TranceGateImpl::set_mix(trance_gate, 1.);
    EXPECT_EQ(trance_gate.mix, 1.);
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_delay_active)
{
    auto trance_gate = TranceGateImpl::create();
    TranceGateImpl::set_sample_rate(trance_gate, real(44100.));
    EXPECT_FALSE(trance_gate.is_delay_active);
    TranceGateImpl::trigger(trance_gate, real(1. / 32.));
    EXPECT_TRUE(trance_gate.is_delay_active);
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_fade_in_active)
{
    auto trance_gate = TranceGateImpl::create();
    TranceGateImpl::set_sample_rate(trance_gate, real(44100.));
    EXPECT_FALSE(trance_gate.is_fade_in_active);
    TranceGateImpl::trigger(trance_gate, real(0.), real(1. / 32.));
    EXPECT_TRUE(trance_gate.is_fade_in_active);
}

//-----------------------------------------------------------------------------
TEST(trance_gate_test, test_processing)
{
    constexpr i32 LEFT_CH = 0;
    auto trance_gate      = TranceGateImpl::create();
    TranceGateImpl::set_sample_rate(trance_gate, real(44100.));
    TranceGateImpl::set_step_count(trance_gate, 8);
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 0, real(1.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 1, real(0.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 2, real(1.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 3, real(0.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 4, real(1.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 5, real(1.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 6, real(1.));
    TranceGateImpl::set_step(trance_gate, LEFT_CH, 7, real(0.));
    TranceGateImpl::set_step_len(trance_gate, real(1. / 32.));

    mut_i32 numSamples = 1024;
    AudioFrame sum{real(0.), real(0.), real(0.), real(0.)};
    while (numSamples-- > 0)
    {
        AudioFrame in{real(0.5), real(0.5), real(0.), real(0.)};
        AudioFrame out{real(0.), real(0.), real(0.), real(0.)};
        TranceGateImpl::process(trance_gate, in, out);
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
