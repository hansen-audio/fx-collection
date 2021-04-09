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
trance_gate::context trance_gate::create()
{
    using phs = ha::dtb::modulation::phase;

    context ctx;

    ctx.contour_filters.resize(NUM_CHANNELS);
    ctx.channel_steps.resize(NUM_CHANNELS);
    for (auto& step : ctx.channel_steps)
        step.resize(MAX_NUM_STEPS, 0);

    constexpr real ONE_32TH = real(1. / 32.);

    phs::set_rate(ctx.delay_phase_ctx, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(ctx.delay_phase_ctx, phs::MODE_TEMPO_SYNC);

    phs::set_rate(ctx.fade_in_phase_ctx, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(ctx.fade_in_phase_ctx, phs::MODE_TEMPO_SYNC);

    phs::set_rate(ctx.step_phase_ctx, phs::note_length_to_rate(ONE_32TH));
    phs::set_mode(ctx.step_phase_ctx, phs::MODE_TEMPO_SYNC);

    constexpr real TEMPO_BPM = real(120.);
    set_tempo(ctx, TEMPO_BPM);

    return ctx;
}

//------------------------------------------------------------------------
void trance_gate::trigger(context& ctx, real delay_length, real fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(ctx, delay_length);
    set_fade_in(ctx, fade_in_length);

    ctx.delay_phase_val    = real(0.);
    ctx.fade_in_phase_val  = real(0.);
    ctx.step_phase_val     = real(0.);
    ctx.step_pos_val.first = 0;

    /*	Do not reset filters in trigger. Because there can still be a voice
        in release playing back. Dann bricht auf einmal Audio weg wenn wir
        hier die Filer auf null setzen. Das klingt komisch dann. Also muss
        das von draussen gesteuert werden, dass wenn Stille herrscht, die Filter
        genullt bzw zurückgesetzt werden ([reset] Methode aufrufen).

        Es gibt eine Ausnahme, nämlich wenn das TG delay an ist. Dann werden
        die Filter auf 1.0 resetted und das muss auch im ::trigger passieren.
    */

    if (ctx.is_delay_active)
        reset(ctx);
}

//------------------------------------------------------------------------
void trance_gate::reset(context& ctx)
{
    /*  Wenn Delay aktiv ist müssen die Filter auf 1.f resetted werden.
        Ansonsten klickt das leicht, weil die Filter nach dem Delay
        erst ganz kurz von 0.f - 1.f einschwingen müssen. Das hört man in Form
        eines Klickens.
    */
    real reset_value = ctx.is_delay_active ? real(1.) : real(0.);
    real pole        = dtb::filtering::one_pole_filter::tau_to_pole(reset_value, ctx.sample_rate);
    for (auto& filter : ctx.contour_filters)
    {
        dtb::filtering::one_pole_filter::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::process(context& ctx, audio_frame_t const& in, audio_frame_t& out)
{
    using osp = ha::dtb::modulation::one_shot_phase;

    // When delay is active and delay_phase has not yet overflown, just pass through.
    bool const is_overflow =
        osp::update_one_shot(ctx.delay_phase_ctx, ctx.delay_phase_val, ONE_SAMPLE);
    if (ctx.is_delay_active && !is_overflow)
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    mut_real value_le = ctx.channel_steps.at(L).at(ctx.step_pos_val.first);
    mut_real value_ri = ctx.channel_steps.at(ctx.ch).at(ctx.step_pos_val.first);

    // Keep order here. Mix must be applied last. The filters smooth everything, cracklefree.
    apply_width(value_le, value_ri, ctx.width);
    apply_contour(value_le, value_ri, ctx.contour_filters);
    real tmp_mix = ctx.is_fade_in_active ? ctx.mix * ctx.fade_in_phase_val : ctx.mix;
    apply_mix(value_le, value_ri, tmp_mix);

    out[L] = in[L] * value_le;
    out[R] = in[R] * value_ri;

    update_phases(ctx);
}

//------------------------------------------------------------------------
void trance_gate::update_phases(context& ctx)
{
    using osp = ha::dtb::modulation::one_shot_phase;
    using phs = ha::dtb::modulation::phase;

    osp::update_one_shot(ctx.fade_in_phase_ctx, ctx.fade_in_phase_val, ONE_SAMPLE);

    // When step_phase has overflown, increment step.
    bool const is_overflow = phs::update(ctx.step_phase_ctx, ctx.step_phase_val, ONE_SAMPLE);
    if (is_overflow)
        ++ctx.step_pos_val;
}

//------------------------------------------------------------------------
void trance_gate::set_sample_rate(context& ctx, real value)
{
    using phs = ha::dtb::modulation::phase;
    using opf = dtb::filtering::one_pole_filter;

    phs::set_sample_rate(ctx.delay_phase_ctx, value);
    phs::set_sample_rate(ctx.fade_in_phase_ctx, value);
    phs::set_sample_rate(ctx.step_phase_ctx, value);

    ctx.sample_rate = value;

    for (auto& filter : ctx.contour_filters)
    {
        real pole = opf::tau_to_pole(ctx.contour, ctx.sample_rate);
        opf::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_step(context& ctx, i32 channel, i32 step, real value_normalised)
{
    ctx.channel_steps.at(channel).at(step) = value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_width(context& ctx, real value_normalised)
{
    ctx.width = real(1.) - value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_stereo_mode(context& ctx, bool value)
{
    ctx.ch = value ? R : L;
}

//------------------------------------------------------------------------
void trance_gate::set_step_length(context& ctx, real value_note_length)
{
    using phs = ha::dtb::modulation::phase;

    phs::set_note_length(ctx.step_phase_ctx, value_note_length);
}
//------------------------------------------------------------------------
void trance_gate::set_tempo(context& ctx, real value)
{
    using phs = ha::dtb::modulation::phase;

    phs::set_tempo(ctx.delay_phase_ctx, value);
    phs::set_tempo(ctx.fade_in_phase_ctx, value);
    phs::set_tempo(ctx.step_phase_ctx, value);
}

//------------------------------------------------------------------------
void trance_gate::set_step_count(context& ctx, i32 value)
{
    ctx.step_pos_val.second = value;
    ctx.step_pos_val.second = std::clamp(ctx.step_pos_val.second, MIN_NUM_STEPS, MAX_NUM_STEPS);
}

//------------------------------------------------------------------------
void trance_gate::set_contour(context& ctx, real value_seconds)
{
    using opf = dtb::filtering::one_pole_filter;

    if (ctx.contour == value_seconds)
        return;

    ctx.contour = value_seconds;
    for (auto& filter : ctx.contour_filters)
    {
        real pole = opf::tau_to_pole(ctx.contour, ctx.sample_rate);
        opf::update_pole(pole, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_fade_in(context& ctx, real value)
{
    using phs = ha::dtb::modulation::phase;

    ctx.is_fade_in_active = value > real(0.);
    if (!ctx.is_fade_in_active)
        return;

    phs::set_note_length(ctx.fade_in_phase_ctx, value);
}

//------------------------------------------------------------------------
void trance_gate::set_delay(context& ctx, real value)
{
    using phs = ha::dtb::modulation::phase;

    ctx.is_delay_active = value > real(0.);
    if (!ctx.is_delay_active)
        return;

    phs::set_note_length(ctx.delay_phase_ctx, value);
}

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha
