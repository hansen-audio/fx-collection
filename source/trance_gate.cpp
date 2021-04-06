// Copyright(c) 2016 René Hansen.

#include "ha/fx_collection/trance_gate.h"
#include <algorithm>

namespace ha {
namespace fx_collection {

//------------------------------------------------------------------------
static constexpr i32 ONE_SAMPLE = 1;

//------------------------------------------------------------------------
static void apply_width(real_t& value_le, real_t& value_ri, real_t width)
{
    value_le = std::max(value_le, value_ri * width);
    value_ri = std::max(value_ri, value_le * width);
}

//------------------------------------------------------------------------
static void apply_mix(real_t& value, real_t mix)
{
    static constexpr real_t MIX_MAX = real_t(1.);
    value                           = (MIX_MAX - mix) + value * mix;
}

//------------------------------------------------------------------------
static void apply_mix(real_t& value_le, real_t& value_ri, real_t mix)
{
    apply_mix(value_le, mix);
    apply_mix(value_ri, mix);
}

//------------------------------------------------------------------------
static void
apply_contour(real_t& value_le, real_t& value_ri, trance_gate::contour_filters_list& contour)
{
    using opf = dtb::filtering::one_pole_filter;

    value_le = opf::process(value_le, contour.at(trance_gate::L));
    value_ri = opf::process(value_ri, contour.at(trance_gate::R));
}

//------------------------------------------------------------------------
static void operator++(trance_gate::step_pos& step)
{
    ++step.first;
    if (!(step.first < step.second))
        step.first = 0;
}

//------------------------------------------------------------------------
//	trance_gate
//------------------------------------------------------------------------
trance_gate::trance_gate()
{
    using phs = ha::dtb::modulation::phase;

    contour_filters.resize(NUM_CHANNELS);
    channel_steps.resize(NUM_CHANNELS);
    for (auto& step : channel_steps)
        step.resize(MAX_NUM_STEPS, real_t(0.));

    constexpr real_t ONE_32TH  = real_t(1. / 32.);
    constexpr real_t TEMPO_BPM = real_t(120.);

    phs::set_rate(delay_phase, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(delay_phase, phs::MODE_TEMPO_SYNC);

    phs::set_rate(fade_in_phase, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(fade_in_phase, phs::MODE_TEMPO_SYNC);

    phs::set_rate(step_phase, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(step_phase, phs::MODE_TEMPO_SYNC);

    set_tempo(TEMPO_BPM);
}

//------------------------------------------------------------------------
void trance_gate::trigger(real_t delay_length, real_t fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(delay_length);
    set_fade_in(fade_in_length);

    delay_phase_value   = real_t(0.);
    fade_in_phase_value = real_t(0.);
    step_phase_value    = real_t(0.);
    step.first          = 0;

    /*	Do not reset filters in trigger. Because there can still be a voice
        in release playing back. Dann bricht auf einmal Audio weg wenn wir
        hier die Filer auf null setzen. Das klingt komisch dann. Also muss
        das von draussen gesteuert werden, dass wenn Stille herrscht, die Filter
        genullt bzw zurückgesetzt werden ([reset] Methode aufrufen).

        Es gibt eine Ausnahme, nämlich wenn das TG delay an ist. Dann werden
        die Filter auf 1.0 resetted und das muss auch im ::trigger passieren.
    */

    if (is_delay_active)
        reset();
}

//------------------------------------------------------------------------
void trance_gate::reset()
{
    /*  Wenn Delay aktiv ist müssen die Filter auf 1.f resetted werden.
        Ansonsten klickt das leicht, weil die Filter nach dem Delay
        erst ganz kurz von 0.f - 1.f einschwingen müssen. Das hört man in Form
        eines Klickens.
    */
    real_t const reset_value = is_delay_active ? real_t(1.) : real_t(0.);
    real_t const pole = dtb::filtering::one_pole_filter::tau_to_pole(reset_value, sample_rate);
    for (auto& filter : contour_filters)
    {
        dtb::filtering::one_pole_filter::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::process(audio_frame_t const& in, audio_frame_t& out)
{
    using osp = ha::dtb::modulation::one_shot_phase;

    // When delay is active and delay_phase has not yet overflown, just pass through.
    bool const is_overflow = osp::update_one_shot(delay_phase, delay_phase_value, ONE_SAMPLE);
    if (is_delay_active && !is_overflow)
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    real_t value_le = channel_steps.at(L).at(step.first);
    real_t value_ri = channel_steps.at(ch).at(step.first);

    // Keep order here. Mix must be applied last. The filters smooth everything, cracklefree.
    apply_width(value_le, value_ri, width);
    apply_contour(value_le, value_ri, contour_filters);
    real_t const tmp_mix = is_fade_in_active ? mix * fade_in_phase_value : mix;
    apply_mix(value_le, value_ri, tmp_mix);

    out[L] = in[L] * value_le;
    out[R] = in[R] * value_ri;

    update_phases();
}

//------------------------------------------------------------------------
void trance_gate::update_phases()
{
    using osp = ha::dtb::modulation::one_shot_phase;
    using phs = ha::dtb::modulation::phase;

    osp::update_one_shot(fade_in_phase, fade_in_phase_value, ONE_SAMPLE);

    // When step_phase has overflown, increment step.
    bool const is_overflow = phs::update(step_phase, step_phase_value, ONE_SAMPLE);
    if (is_overflow)
        ++step;
}

//------------------------------------------------------------------------
void trance_gate::set_sample_rate(real_t value)
{
    using phs = ha::dtb::modulation::phase;
    using opf = dtb::filtering::one_pole_filter;

    phs::set_sample_rate(delay_phase, value);
    phs::set_sample_rate(fade_in_phase, value);
    phs::set_sample_rate(step_phase, value);

    sample_rate = value;

    for (auto& filter : contour_filters)
    {
        real_t const pole = opf::tau_to_pole(contour, sample_rate);
        opf::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_step(i32 channel, i32 step, real_t value_normalised)
{
    channel_steps.at(channel).at(step) = value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_width(real_t value_normalised)
{
    width = real_t(1.) - value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_stereo_mode(bool value)
{
    ch = value ? R : L;
}

//------------------------------------------------------------------------
void trance_gate::set_step_length(real_t value_note_length)
{
    using phs = ha::dtb::modulation::phase;

    phs::set_note_length(step_phase, value_note_length);
}
//------------------------------------------------------------------------
void trance_gate::set_tempo(real_t value)
{
    using phs = ha::dtb::modulation::phase;

    phs::set_tempo(delay_phase, value);
    phs::set_tempo(fade_in_phase, value);
    phs::set_tempo(step_phase, value);
}

//------------------------------------------------------------------------
void trance_gate::set_step_count(i32 value)
{
    constexpr i32 MIN_NUM_STEPS = 1;

    step.second = value;
    step.second = std::clamp(step.second, MIN_NUM_STEPS, MAX_NUM_STEPS);
}

//------------------------------------------------------------------------
void trance_gate::set_contour(real_t value_seconds)
{
    using opf = dtb::filtering::one_pole_filter;

    if (contour == value_seconds)
        return;

    contour = value_seconds;
    for (auto& filter : contour_filters)
    {
        real_t const pole = opf::tau_to_pole(contour, sample_rate);
        opf::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_fade_in(real_t value)
{
    using phs = ha::dtb::modulation::phase;

    is_fade_in_active = value > real_t(0.);
    if (!is_fade_in_active)
        return;

    phs::set_note_length(fade_in_phase, value);
}

//------------------------------------------------------------------------
void trance_gate::set_delay(real_t value)
{
    using phs = ha::dtb::modulation::phase;

    is_delay_active = value > real_t(0.);
    if (!is_delay_active)
        return;

    phs::set_note_length(delay_phase, value);
}

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha
