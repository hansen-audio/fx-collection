// Copyright(c) 2016 René Hansen.

#include "ha/fx_collection/trance_gate.h"
#include "detail/shuffle_note.h"
#include <algorithm>

namespace ha::fx_collection {

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
static void apply_contour(mut_real& value_le,
                          mut_real& value_ri,
                          trance_gate::contour_filters_list& contour)
{
    using opf = dtb::filtering::one_pole_filter;

    value_le = opf::process(contour.at(trance_gate::L), value_le);
    value_ri = opf::process(contour.at(trance_gate::R), value_ri);
}

//------------------------------------------------------------------------
static void apply_gate_delay(mut_real& value_le,
                             mut_real& value_ri,
                             real phase_value,
                             real delay)
{
    real factor = phase_value > delay ? real(1.) : real(0.);
    value_le *= factor;
    value_ri *= factor;
}

//------------------------------------------------------------------------
static void apply_shuffle(mut_real& value_le,
                          mut_real& value_ri,
                          real phase_value,
                          trance_gate::context const& cx)
{
    // TODO: Is this a good value for a MAX_DELAY?
    constexpr real MAX_DELAY = real(3. / 4.);
    real delay               = cx.shuffle * MAX_DELAY;

    if (cx.step_val.is_shuffle)
        apply_gate_delay(value_le, value_ri, phase_value, delay);
}

//------------------------------------------------------------------------
static void set_shuffle(trance_gate::context::step& s, real note_len)
{
    s.is_shuffle = detail::is_shuffle_note(s.pos, note_len);
}

//------------------------------------------------------------------------
static void operator++(trance_gate::context::step& s)
{
    ++s.pos;
    if (!(s.pos < s.count))
        s.pos = 0;
}

//------------------------------------------------------------------------
//	trance_gate
//------------------------------------------------------------------------
trance_gate::context trance_gate::create()
{
    using phs = dtb::modulation::phase;

    context cx;

    constexpr real INIT_NOTE_LEN = real(1. / 32.);

    phs::set_rate(cx.delay_phase_cx, phs::note_length_to_rate(INIT_NOTE_LEN));
    phs::set_sync_mode(cx.delay_phase_cx, phs::sync_mode::PROJECT_SYNC);

    phs::set_rate(cx.fade_in_phase_cx, phs::note_length_to_rate(INIT_NOTE_LEN));
    phs::set_sync_mode(cx.fade_in_phase_cx, phs::sync_mode::PROJECT_SYNC);

    phs::set_rate(cx.step_phase_cx, phs::note_length_to_rate(INIT_NOTE_LEN));
    phs::set_sync_mode(cx.step_phase_cx, phs::sync_mode::PROJECT_SYNC);

    constexpr real TEMPO_BPM = real(120.);
    set_tempo(cx, TEMPO_BPM);

    return cx;
}

//------------------------------------------------------------------------
void trance_gate::trigger(context& cx, real delay_length, real fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(cx, delay_length);
    set_fade_in(cx, fade_in_length);

    cx.delay_phase_val   = real(0.);
    cx.fade_in_phase_val = real(0.);
    cx.step_phase_val    = real(0.);
    cx.step_val.pos      = 0;

    /*	Do not reset filters in trigger. Because there can still be a voice
        in release playing back. Dann bricht auf einmal Audio weg wenn wir
        hier die Filer auf null setzen. Das klingt komisch dann. Also muss
        das von draussen gesteuert werden, dass wenn Stille herrscht, die Filter
        genullt bzw zurückgesetzt werden ([reset] Methode aufrufen).

        Es gibt eine Ausnahme, nämlich wenn das TG delay an ist. Dann werden
        die Filter auf 1.0 resetted und das muss auch im ::trigger passieren.
    */

    if (cx.is_delay_active)
        reset(cx);
}

//------------------------------------------------------------------------
void trance_gate::reset(context& cx)
{
    /*  Wenn Delay aktiv ist müssen die Filter auf 1.f resetted werden.
        Ansonsten klickt das leicht, weil die Filter nach dem Delay
        erst ganz kurz von 0.f - 1.f einschwingen müssen. Das hört man in Form
        eines Klickens.
    */
    real reset_value = cx.is_delay_active ? real(1.) : real(0.);
    for (auto& filt_cx : cx.contour_filters)
    {
        dtb::filtering::one_pole_filter::reset(filt_cx, reset_value);
    }
}

//------------------------------------------------------------------------
void trance_gate::process(context& cx, audio_frame const& in, audio_frame& out)
{
    using phs = dtb::modulation::phase;

    // When delay is active and delay_phase has not yet overflown, just pass
    // through.
    bool const is_overflow = phs::advance_one_shot(
        cx.delay_phase_cx, cx.delay_phase_val, ONE_SAMPLE);
    if (cx.is_delay_active && !is_overflow)
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    mut_real value_le = cx.channel_steps.at(L).at(cx.step_val.pos);
    mut_real value_ri = cx.channel_steps.at(cx.ch).at(cx.step_val.pos);

    // Keep order here. Mix must be applied last. The filters smooth everything,
    // cracklefree.
    apply_shuffle(value_le, value_ri, cx.step_phase_val, cx);
    apply_width(value_le, value_ri, cx.width);
    apply_contour(value_le, value_ri, cx.contour_filters);
    real tmp_mix =
        cx.is_fade_in_active ? cx.mix * cx.fade_in_phase_val : cx.mix;
    apply_mix(value_le, value_ri, tmp_mix);

    out.data[L] = in.data[L] * value_le;
    out.data[R] = in.data[R] * value_ri;

    update_phases(cx);
}

//------------------------------------------------------------------------
void trance_gate::update_phases(context& cx)
{
    using phs = dtb::modulation::phase;

    phs::advance_one_shot(cx.fade_in_phase_cx, cx.fade_in_phase_val,
                          ONE_SAMPLE);

    // When step_phase has overflown, increment step.
    bool const is_overflow =
        phs::advance(cx.step_phase_cx, cx.step_phase_val, ONE_SAMPLE);
    if (is_overflow)
    {
        ++cx.step_val;
        set_shuffle(cx.step_val, cx.step_phase_cx.note_len);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_sample_rate(context& cx, real value)
{
    using phs = dtb::modulation::phase;
    using opf = dtb::filtering::one_pole_filter;

    phs::set_sample_rate(cx.delay_phase_cx, value);
    phs::set_sample_rate(cx.fade_in_phase_cx, value);
    phs::set_sample_rate(cx.step_phase_cx, value);

    cx.sample_rate = value;

    for (auto& filt_cx : cx.contour_filters)
    {
        real pole = opf::tau_to_pole(cx.contour, cx.sample_rate);
        opf::update_pole(filt_cx, pole);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_step(context& cx,
                           i32 channel,
                           i32 step,
                           real value_normalised)
{
    cx.channel_steps.at(channel).at(step) = value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_width(context& cx, real value_normalised)
{
    cx.width = real(1.) - value_normalised;
}

//------------------------------------------------------------------------
void trance_gate::set_shuffle_amount(context& cx, real value)
{
    cx.shuffle = value;
}

//------------------------------------------------------------------------
void trance_gate::set_stereo_mode(context& cx, bool value)
{
    cx.ch = value ? R : L;
}

//------------------------------------------------------------------------
void trance_gate::set_step_len(context& cx, real value_note_len)
{
    using phs = dtb::modulation::phase;

    phs::set_note_len(cx.step_phase_cx, value_note_len);
}

//------------------------------------------------------------------------
void trance_gate::set_tempo(context& cx, real value)
{
    using phs = dtb::modulation::phase;

    phs::set_tempo(cx.delay_phase_cx, value);
    phs::set_tempo(cx.fade_in_phase_cx, value);
    phs::set_tempo(cx.step_phase_cx, value);
}

//------------------------------------------------------------------------
void trance_gate::update_project_time_music(context& cx, real value)
{
    using phs = dtb::modulation::phase;

    phs::set_project_time(cx.delay_phase_cx, value);
    phs::set_project_time(cx.fade_in_phase_cx, value);
    phs::set_project_time(cx.step_phase_cx, value);
}
//------------------------------------------------------------------------
void trance_gate::set_step_count(context& cx, i32 value)
{
    cx.step_val.count = value;
    cx.step_val.count =
        std::clamp(cx.step_val.count, MIN_NUM_STEPS, MAX_NUM_STEPS);
}

//------------------------------------------------------------------------
void trance_gate::set_contour(context& cx, real value_seconds)
{
    using opf = dtb::filtering::one_pole_filter;

    if (cx.contour == value_seconds)
        return;

    cx.contour = value_seconds;
    for (auto& filt_cx : cx.contour_filters)
    {
        real pole = opf::tau_to_pole(cx.contour, cx.sample_rate);
        opf::update_pole(filt_cx, pole);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_fade_in(context& cx, real value)
{
    using phs = dtb::modulation::phase;

    cx.is_fade_in_active = value > real(0.);
    if (!cx.is_fade_in_active)
        return;

    phs::set_note_len(cx.fade_in_phase_cx, value);
}

//------------------------------------------------------------------------
void trance_gate::set_delay(context& cx, real value)
{
    using phs = dtb::modulation::phase;

    cx.is_delay_active = value > real(0.);
    if (!cx.is_delay_active)
        return;

    phs::set_note_len(cx.delay_phase_cx, value);
}

//------------------------------------------------------------------------
} // namespace ha::fx_collection
