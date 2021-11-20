// Copyright(c) 2016 René Hansen.

#include "ha/fx_collection/trance_gate.h"
#include "detail/shuffle_note.h"
#include <algorithm>

namespace ha::fx_collection {

//------------------------------------------------------------------------
static constexpr i32 ONE_SAMPLE = 1;

//------------------------------------------------------------------------
static void
apply_width(TranceGate const& trance_gate, mut_f32& value_le, mut_f32& value_ri)
{
    value_le = std::max(value_le, value_ri * trance_gate.width);
    value_ri = std::max(value_ri, value_le * trance_gate.width);
}

//------------------------------------------------------------------------
static void apply_mix(mut_f32& value, f32 mix)
{
    static constexpr f32 MIX_MAX = f32(1.);
    value                        = (MIX_MAX - mix) + value * mix;
}

//------------------------------------------------------------------------
static void apply_mix(f32 mix, mut_f32& value_le, mut_f32& value_ri)
{
    apply_mix(value_le, mix);
    apply_mix(value_ri, mix);
}

//------------------------------------------------------------------------
static void
apply_contour(TranceGate& trance_gate, mut_f32& value_le, mut_f32& value_ri)
{
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    auto& contour_filters = trance_gate.contour_filters;
    value_le =
        OnePoleImpl::process(contour_filters.at(TranceGate::L), value_le);
    value_ri =
        OnePoleImpl::process(contour_filters.at(TranceGate::R), value_ri);
}

//------------------------------------------------------------------------
static f32 compute_mix(TranceGate const& trance_gate)
{
    return trance_gate.is_fade_in_active
               ? trance_gate.mix * trance_gate.fade_in_phase_val
               : trance_gate.mix;
}

//------------------------------------------------------------------------
static void
apply_mix(TranceGate& trance_gate, mut_f32& value_le, mut_f32& value_ri)
{
    f32 tmp_mix = compute_mix(trance_gate);
    apply_mix(tmp_mix, value_le, value_ri);
}

//------------------------------------------------------------------------
static void apply_gate_delay(mut_f32& value_le,
                             mut_f32& value_ri,
                             f32 phase_value,
                             f32 delay)
{
    f32 factor = phase_value > delay ? f32(1.) : f32(0.);
    value_le *= factor;
    value_ri *= factor;
}

//------------------------------------------------------------------------
static void apply_shuffle(TranceGate const& trance_gate,
                          mut_f32& value_le,
                          mut_f32& value_ri)
{
    // TODO: Is this a good value for a MAX_DELAY?
    constexpr f32 MAX_DELAY = f32(3. / 4.);
    f32 delay               = trance_gate.shuffle * MAX_DELAY;

    if (trance_gate.step_val.is_shuffle)
        apply_gate_delay(value_le, value_ri, trance_gate.step_phase_val, delay);
}

//------------------------------------------------------------------------
static void apply_trance_gate_fx(TranceGate& trance_gate,
                                 mut_f32& value_le,
                                 mut_f32& value_ri)
{
    // Keep order here. Mix must be applied last. The filters smooth everything,
    // cracklefree.
    apply_shuffle(trance_gate, value_le, value_ri);
    apply_width(trance_gate, value_le, value_ri);
    apply_contour(trance_gate, value_le, value_ri);
    apply_mix(trance_gate, value_le, value_ri);
}

//------------------------------------------------------------------------
static void set_shuffle(TranceGate::Step& s, f32 note_len)
{
    s.is_shuffle = detail::is_shuffle_note(s.pos, note_len);
}

//------------------------------------------------------------------------
static void operator++(TranceGate::Step& s)
{
    ++s.pos;
    if (!(s.pos < s.count))
        s.pos = 0;
}

//------------------------------------------------------------------------
//	TranceGateImpl
//------------------------------------------------------------------------
TranceGate TranceGateImpl::create()
{
    using PhaseImpl = dtb::modulation::PhaseImpl;
    using Phase     = dtb::modulation::Phase;

    TranceGate trance_gate;

    constexpr f32 INIT_NOTE_LEN = f32(1. / 32.);

    PhaseImpl::set_rate(trance_gate.delay_phase,
                        PhaseImpl::note_length_to_rate(INIT_NOTE_LEN));
    PhaseImpl::set_sync_mode(trance_gate.delay_phase,
                             Phase::SyncMode::ProjectSync);

    PhaseImpl::set_rate(trance_gate.fade_in_phase,
                        PhaseImpl::note_length_to_rate(INIT_NOTE_LEN));
    PhaseImpl::set_sync_mode(trance_gate.fade_in_phase,
                             Phase::SyncMode::ProjectSync);

    PhaseImpl::set_rate(trance_gate.step_phase,
                        PhaseImpl::note_length_to_rate(INIT_NOTE_LEN));
    PhaseImpl::set_sync_mode(trance_gate.step_phase,
                             Phase::SyncMode::ProjectSync);

    constexpr f32 TEMPO_BPM = f32(120.);
    set_tempo(trance_gate, TEMPO_BPM);

    return trance_gate;
}

//------------------------------------------------------------------------
void TranceGateImpl::trigger(TranceGate& trance_gate,
                             f32 delay_length,
                             f32 fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(trance_gate, delay_length);
    set_fade_in(trance_gate, fade_in_length);

    trance_gate.delay_phase_val   = f32(0.);
    trance_gate.fade_in_phase_val = f32(0.);
    trance_gate.step_phase_val    = f32(0.);
    trance_gate.step_val.pos      = 0;

    /*	Do not reset filters in trigger. Because there can still be a voice
        in release playing back. Dann bricht auf einmal Audio weg wenn wir
        hier die Filer auf null setzen. Das klingt komisch dann. Also muss
        das von draussen gesteuert werden, dass wenn Stille herrscht, die Filter
        genullt bzw zurückgesetzt werden ([reset] Methode aufrufen).

        Es gibt eine Ausnahme, nämlich wenn das TG delay an ist. Dann werden
        die Filter auf 1.0 resetted und das muss auch im ::trigger passieren.
    */

    if (trance_gate.is_delay_active)
        reset(trance_gate);
}

//------------------------------------------------------------------------
void TranceGateImpl::reset(TranceGate& trance_gate)
{
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    /*  Wenn Delay aktiv ist müssen die Filter auf 1.f resetted werden.
        Ansonsten klickt das leicht, weil die Filter nach dem Delay
        erst ganz kurz von 0.f - 1.f einschwingen müssen. Das hört man in Form
        eines Klickens.
    */
    f32 reset_value = trance_gate.is_delay_active ? f32(1.) : f32(0.);
    for (auto& filter : trance_gate.contour_filters)
    {
        OnePoleImpl::reset(filter, reset_value);
    }
}

//------------------------------------------------------------------------
void TranceGateImpl::reset_step_pos(TranceGate& trance_gate, i32 value)
{
    trance_gate.step_val.pos = value;
}

//------------------------------------------------------------------------
i32 TranceGateImpl::get_step_pos(const TranceGate& trance_gate)
{
    return trance_gate.step_val.pos;
}

//------------------------------------------------------------------------
void TranceGateImpl::process(TranceGate& trance_gate,
                             AudioFrame const& in,
                             AudioFrame& out)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    // When delay is active and delay_phase has not yet overflown, just pass
    // through.
    bool const is_overflow = PhaseImpl::advance_one_shot(
        trance_gate.delay_phase, trance_gate.delay_phase_val, ONE_SAMPLE);
    if (trance_gate.is_delay_active && !is_overflow)
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    i32 pos          = trance_gate.step_val.pos;
    mut_f32 value_le = trance_gate.channel_steps[TranceGate::L][pos];
    mut_f32 value_ri = trance_gate.channel_steps[trance_gate.ch][pos];

    apply_trance_gate_fx(trance_gate, value_le, value_ri);

    out.data[TranceGate::L] = in.data[TranceGate::L] * value_le;
    out.data[TranceGate::R] = in.data[TranceGate::R] * value_ri;

    update_phases(trance_gate);
}

//------------------------------------------------------------------------
void TranceGateImpl::update_phases(TranceGate& trance_gate)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    PhaseImpl::advance_one_shot(trance_gate.fade_in_phase,
                                trance_gate.fade_in_phase_val, ONE_SAMPLE);

    // When step_phase has overflown, increment step.
    bool const is_overflow = PhaseImpl::advance(
        trance_gate.step_phase, trance_gate.step_phase_val, ONE_SAMPLE);
    if (is_overflow)
    {
        ++trance_gate.step_val;
        set_shuffle(trance_gate.step_val, trance_gate.step_phase.note_len);
    }
}

//------------------------------------------------------------------------
void TranceGateImpl::set_sample_rate(TranceGate& trance_gate, f32 value)
{
    using PhaseImpl   = dtb::modulation::PhaseImpl;
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    PhaseImpl::set_sample_rate(trance_gate.delay_phase, value);
    PhaseImpl::set_sample_rate(trance_gate.fade_in_phase, value);
    PhaseImpl::set_sample_rate(trance_gate.step_phase, value);

    trance_gate.sample_rate = value;

    for (auto& filter : trance_gate.contour_filters)
    {
        f32 pole = OnePoleImpl::tau_to_pole(trance_gate.contour,
                                            trance_gate.sample_rate);
        OnePoleImpl::update_pole(filter, pole);
    }
}

//------------------------------------------------------------------------
void TranceGateImpl::set_step(TranceGate& trance_gate,
                              i32 channel,
                              i32 step,
                              f32 value_normalised)
{
    trance_gate.channel_steps.at(channel).at(step) = value_normalised;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_width(TranceGate& trance_gate, f32 value_normalised)
{
    trance_gate.width = f32(1.) - value_normalised;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_shuffle_amount(TranceGate& trance_gate, f32 value)
{
    trance_gate.shuffle = value;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_stereo_mode(TranceGate& trance_gate, bool value)
{
    trance_gate.ch = value ? TranceGate::R : TranceGate::L;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_step_len(TranceGate& trance_gate, f32 value_note_len)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    PhaseImpl::set_note_len(trance_gate.step_phase, value_note_len);
}

//------------------------------------------------------------------------
void TranceGateImpl::set_tempo(TranceGate& trance_gate, f32 value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    PhaseImpl::set_tempo(trance_gate.delay_phase, value);
    PhaseImpl::set_tempo(trance_gate.fade_in_phase, value);
    PhaseImpl::set_tempo(trance_gate.step_phase, value);
}

//------------------------------------------------------------------------
void TranceGateImpl::update_project_time_music(TranceGate& trance_gate,
                                               f64 value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    PhaseImpl::set_project_time(trance_gate.delay_phase, value);
    PhaseImpl::set_project_time(trance_gate.fade_in_phase, value);
    PhaseImpl::set_project_time(trance_gate.step_phase, value);
}

//------------------------------------------------------------------------
void TranceGateImpl::set_step_count(TranceGate& trance_gate, i32 value)
{
    trance_gate.step_val.count = value;
    trance_gate.step_val.count =
        std::clamp(trance_gate.step_val.count, TranceGate::MIN_NUM_STEPS,
                   TranceGate::MAX_NUM_STEPS);
}

//------------------------------------------------------------------------
void TranceGateImpl::set_contour(TranceGate& trance_gate, f32 value_seconds)
{
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    if (trance_gate.contour == value_seconds)
        return;

    trance_gate.contour = value_seconds;
    for (auto& filter : trance_gate.contour_filters)
    {
        f32 pole = OnePoleImpl::tau_to_pole(trance_gate.contour,
                                            trance_gate.sample_rate);
        OnePoleImpl::update_pole(filter, pole);
    }
}

//------------------------------------------------------------------------
void TranceGateImpl::set_fade_in(TranceGate& trance_gate, f32 value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    trance_gate.is_fade_in_active = value > f32(0.);
    if (!trance_gate.is_fade_in_active)
        return;

    PhaseImpl::set_note_len(trance_gate.fade_in_phase, value);
}

//------------------------------------------------------------------------
void TranceGateImpl::set_delay(TranceGate& trance_gate, f32 value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    trance_gate.is_delay_active = value > f32(0.);
    if (!trance_gate.is_delay_active)
        return;

    PhaseImpl::set_note_len(trance_gate.delay_phase, value);
}

//------------------------------------------------------------------------
} // namespace ha::fx_collection
