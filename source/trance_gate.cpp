// Copyright(c) 2016 René Hansen.

#include "ha/fx_collection/trance_gate.h"
#include <algorithm>

namespace ha {
namespace fx_collection {

//------------------------------------------------------------------------
static constexpr i32 ONE_SAMPLE = 1;

//------------------------------------------------------------------------
static void apply_width(mut_real& value_le, mut_real& value_ri, real width)
{
    value_le = std::max(value_le, value_ri * width);
    value_ri = std::max(value_ri, value_le * width);
}

//------------------------------------------------------------------------
static void apply_mix(mut_real& value, real mix)
{
    static constexpr real MIX_MAX = real(1.);
    value                         = (MIX_MAX - mix) + value * mix;
}

//------------------------------------------------------------------------
static void apply_mix(mut_real& value_le, mut_real& value_ri, real mix)
{
    apply_mix(value_le, mix);
    apply_mix(value_ri, mix);
}

//------------------------------------------------------------------------
static void
apply_contour(mut_real& value_le, mut_real& value_ri, trance_gate::contour_filters_list& contour)
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
        step.resize(MAX_NUM_STEPS, 0);

    constexpr real ONE_32TH  = real(1. / 32.);
    constexpr real TEMPO_BPM = real(120.);

    phs::set_rate(delay_phase_ctx, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(delay_phase_ctx, phs::MODE_TEMPO_SYNC);

    phs::set_rate(fade_in_phase_ctx, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(fade_in_phase_ctx, phs::MODE_TEMPO_SYNC);

    phs::set_rate(step_phase_ctx, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(step_phase_ctx, phs::MODE_TEMPO_SYNC);

    set_tempo(TEMPO_BPM);
}

//------------------------------------------------------------------------
void trance_gate::trigger(real delay_length, real fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(delay_length);
    set_fade_in(fade_in_length);

    delay_phase_val    = real(0.);
    fade_in_phase_val  = real(0.);
    step_phase_val     = real(0.);
    step_pos_val.first = 0;

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
    real reset_value = is_delay_active ? real(1.) : real(0.);
    real pole        = dtb::filtering::one_pole_filter::tau_to_pole(reset_value, sample_rate);
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
    bool const is_overflow = osp::update_one_shot(delay_phase_ctx, delay_phase_val, ONE_SAMPLE);
    if (is_delay_active && !is_overflow)
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    mut_real value_le = channel_steps.at(L).at(step_pos_val.first);
    mut_real value_ri = channel_steps.at(ch).at(step_pos_val.first);

    // Keep order here. Mix must be applied last. The filters smooth everything, cracklefree.
    apply_width(value_le, value_ri, width);
    apply_contour(value_le, value_ri, contour_filters);
    real tmp_mix = is_fade_in_active ? mix * fade_in_phase_val : mix;
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

    osp::update_one_shot(fade_in_phase_ctx, fade_in_phase_val, ONE_SAMPLE);

    // When step_phase has overflown, increment step.
    bool const is_overflow = phs::update(step_phase_ctx, step_phase_val, ONE_SAMPLE);
    if (is_overflow)
        ++step_pos_val;
}

//------------------------------------------------------------------------
void trance_gate::set_sample_rate(real value)
{
    using phs = ha::dtb::modulation::phase;
    using opf = dtb::filtering::one_pole_filter;

    phs::set_sample_rate(delay_phase_ctx, value);
    phs::set_sample_rate(fade_in_phase_ctx, value);
    phs::set_sample_rate(step_phase_ctx, value);

    sample_rate = value;

    for (auto& filter : contour_filters)
    {
        real pole = opf::tau_to_pole(contour, sample_rate);
        opf::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_step(i32 channel, i32 step, real value_normalised)
{
    channel_steps.at(channel).at(step) = value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_width(real value_normalised)
{
    width = real(1.) - value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_stereo_mode(bool value)
{
    ch = value ? R : L;
}

//------------------------------------------------------------------------
void trance_gate::set_step_length(real value_note_length)
{
    using phs = ha::dtb::modulation::phase;

    phs::set_note_length(step_phase_ctx, value_note_length);
}
//------------------------------------------------------------------------
void trance_gate::set_tempo(real value)
{
    using phs = ha::dtb::modulation::phase;

    phs::set_tempo(delay_phase_ctx, value);
    phs::set_tempo(fade_in_phase_ctx, value);
    phs::set_tempo(step_phase_ctx, value);
}

//------------------------------------------------------------------------
void trance_gate::set_step_count(i32 value)
{
    step_pos_val.second = value;
    step_pos_val.second = std::clamp(step_pos_val.second, MIN_NUM_STEPS, MAX_NUM_STEPS);
}

//------------------------------------------------------------------------
void trance_gate::set_contour(real value_seconds)
{
    using opf = dtb::filtering::one_pole_filter;

    if (contour == value_seconds)
        return;

    contour = value_seconds;
    for (auto& filter : contour_filters)
    {
        real pole = opf::tau_to_pole(contour, sample_rate);
        opf::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_fade_in(real value)
{
    using phs = ha::dtb::modulation::phase;

    is_fade_in_active = value > real(0.);
    if (!is_fade_in_active)
        return;

    phs::set_note_length(fade_in_phase_ctx, value);
}

//------------------------------------------------------------------------
void trance_gate::set_delay(real value)
{
    using phs = ha::dtb::modulation::phase;

    is_delay_active = value > real(0.);
    if (!is_delay_active)
        return;

    phs::set_note_length(delay_phase_ctx, value);
}

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha
