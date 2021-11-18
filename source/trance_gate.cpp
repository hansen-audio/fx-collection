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
                          TranceGate::ContourFilters& contour_filters)
{
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    value_le =
        OnePoleImpl::process(contour_filters.at(TranceGate::L), value_le);
    value_ri =
        OnePoleImpl::process(contour_filters.at(TranceGate::R), value_ri);
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
                          TranceGate const& trance_gate)
{
    // TODO: Is this a good value for a MAX_DELAY?
    constexpr real MAX_DELAY = real(3. / 4.);
    real delay               = trance_gate.shuffle * MAX_DELAY;

    if (trance_gate.step_val.is_shuffle)
        apply_gate_delay(value_le, value_ri, phase_value, delay);
}

//------------------------------------------------------------------------
static void set_shuffle(TranceGate::Step& s, real note_len)
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

    constexpr real INIT_NOTE_LEN = real(1. / 32.);

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

    constexpr real TEMPO_BPM = real(120.);
    set_tempo(trance_gate, TEMPO_BPM);

    return trance_gate;
}

//------------------------------------------------------------------------
void TranceGateImpl::trigger(TranceGate& trance_gate,
                             real delay_length,
                             real fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(trance_gate, delay_length);
    set_fade_in(trance_gate, fade_in_length);

    trance_gate.delay_phase_val   = real(0.);
    trance_gate.fade_in_phase_val = real(0.);
    trance_gate.step_phase_val    = real(0.);
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
    /*  Wenn Delay aktiv ist müssen die Filter auf 1.f resetted werden.
        Ansonsten klickt das leicht, weil die Filter nach dem Delay
        erst ganz kurz von 0.f - 1.f einschwingen müssen. Das hört man in Form
        eines Klickens.
    */
    real reset_value = trance_gate.is_delay_active ? real(1.) : real(0.);
    for (auto& filter : trance_gate.contour_filters)
    {
        dtb::filtering::OnePoleImpl::reset(filter, reset_value);
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
    mut_real value_le = trance_gate.channel_steps.at(TranceGate::L)
                            .at(trance_gate.step_val.pos);
    mut_real value_ri = trance_gate.channel_steps.at(trance_gate.ch)
                            .at(trance_gate.step_val.pos);

    // Keep order here. Mix must be applied last. The filters smooth everything,
    // cracklefree.
    apply_shuffle(value_le, value_ri, trance_gate.step_phase_val, trance_gate);
    apply_width(value_le, value_ri, trance_gate.width);
    apply_contour(value_le, value_ri, trance_gate.contour_filters);
    real tmp_mix = trance_gate.is_fade_in_active
                       ? trance_gate.mix * trance_gate.fade_in_phase_val
                       : trance_gate.mix;
    apply_mix(value_le, value_ri, tmp_mix);

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
void TranceGateImpl::set_sample_rate(TranceGate& trance_gate, real value)
{
    using PhaseImpl   = dtb::modulation::PhaseImpl;
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    PhaseImpl::set_sample_rate(trance_gate.delay_phase, value);
    PhaseImpl::set_sample_rate(trance_gate.fade_in_phase, value);
    PhaseImpl::set_sample_rate(trance_gate.step_phase, value);

    trance_gate.sample_rate = value;

    for (auto& filter : trance_gate.contour_filters)
    {
        real pole = OnePoleImpl::tau_to_pole(trance_gate.contour,
                                             trance_gate.sample_rate);
        OnePoleImpl::update_pole(filter, pole);
    }
}

//------------------------------------------------------------------------
void TranceGateImpl::set_step(TranceGate& trance_gate,
                              i32 channel,
                              i32 step,
                              real value_normalised)
{
    trance_gate.channel_steps.at(channel).at(step) = value_normalised;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_width(TranceGate& trance_gate, real value_normalised)
{
    trance_gate.width = real(1.) - value_normalised;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_shuffle_amount(TranceGate& trance_gate, real value)
{
    trance_gate.shuffle = value;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_stereo_mode(TranceGate& trance_gate, bool value)
{
    trance_gate.ch = value ? TranceGate::R : TranceGate::L;
}

//------------------------------------------------------------------------
void TranceGateImpl::set_step_len(TranceGate& trance_gate, real value_note_len)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    PhaseImpl::set_note_len(trance_gate.step_phase, value_note_len);
}

//------------------------------------------------------------------------
void TranceGateImpl::set_tempo(TranceGate& trance_gate, real value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    PhaseImpl::set_tempo(trance_gate.delay_phase, value);
    PhaseImpl::set_tempo(trance_gate.fade_in_phase, value);
    PhaseImpl::set_tempo(trance_gate.step_phase, value);
}

//------------------------------------------------------------------------
void TranceGateImpl::update_project_time_music(TranceGate& trance_gate,
                                               real value)
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
void TranceGateImpl::set_contour(TranceGate& trance_gate, real value_seconds)
{
    using OnePoleImpl = dtb::filtering::OnePoleImpl;

    if (trance_gate.contour == value_seconds)
        return;

    trance_gate.contour = value_seconds;
    for (auto& filter : trance_gate.contour_filters)
    {
        real pole = OnePoleImpl::tau_to_pole(trance_gate.contour,
                                             trance_gate.sample_rate);
        OnePoleImpl::update_pole(filter, pole);
    }
}

//------------------------------------------------------------------------
void TranceGateImpl::set_fade_in(TranceGate& trance_gate, real value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    trance_gate.is_fade_in_active = value > real(0.);
    if (!trance_gate.is_fade_in_active)
        return;

    PhaseImpl::set_note_len(trance_gate.fade_in_phase, value);
}

//------------------------------------------------------------------------
void TranceGateImpl::set_delay(TranceGate& trance_gate, real value)
{
    using PhaseImpl = dtb::modulation::PhaseImpl;

    trance_gate.is_delay_active = value > real(0.);
    if (!trance_gate.is_delay_active)
        return;

    PhaseImpl::set_note_len(trance_gate.delay_phase, value);
}

//------------------------------------------------------------------------
} // namespace ha::fx_collection
