// Copyright(c) 2016 René Hansen.

#include "ha/fx_collection/trance_gate.h"

namespace ha {
namespace fx_collection {

//------------------------------------------------------------------------
static void apply_width(float_t& value_le, float_t& value_ri, float_t width)
{
    value_le = std::max(value_le, value_ri * width);
    value_ri = std::max(value_ri, value_le * width);
}

//------------------------------------------------------------------------
static void apply_mix(float_t& value, float_t mix)
{
    static constexpr float_t MIX_MAX = float_t(1.);
    value                            = (MIX_MAX - mix) + value * mix;
}

//------------------------------------------------------------------------
static void apply_mix(float_t& value_le, float_t& value_ri, float_t mix)
{
    apply_mix(value_le, mix);
    apply_mix(value_ri, mix);
}

//------------------------------------------------------------------------
static void
apply_contour(float_t& value_le, float_t& value_ri, trance_gate::contour_filters_list& contour)
{
    value_le = contour_filter::process(value_le, contour.at(trance_gate::L));
    value_ri = contour_filter::process(value_ri, contour.at(trance_gate::R));
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
    contour_filters.resize(NUM_CHANNELS);
    channel_steps.resize(NUM_CHANNELS);
    for (auto& step : channel_steps)
        step.resize(NUM_STEPS, float_t(0.));

    constexpr float_t ONE_32TH  = float_t(1. / 32.);
    constexpr float_t TEMPO_BPM = float_t(120.);

    delay_phase.set_rate(ha::dtb::modulation::phase::note_length_to_rate(ONE_32TH));
    delay_phase.set_tempo(TEMPO_BPM);
    delay_phase.set_mode(ha::dtb::modulation::phase::MODE_TEMPO_SYNC);

    fade_in_phase.set_rate(ha::dtb::modulation::phase::note_length_to_rate(ONE_32TH));
    fade_in_phase.set_tempo(TEMPO_BPM);
    fade_in_phase.set_mode(ha::dtb::modulation::phase::MODE_TEMPO_SYNC);

    step_phase.set_rate(ha::dtb::modulation::phase::note_length_to_rate(ONE_32TH));
    step_phase.set_tempo(TEMPO_BPM);
    step_phase.set_mode(ha::dtb::modulation::phase::MODE_TEMPO_SYNC);
}

//------------------------------------------------------------------------
void trance_gate::trigger(float_t delay_length, float_t fade_in_length)
{
    //! Delay and FadeIn time can only be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    set_delay(delay_length);
    set_fade_in(fade_in_length);

    delay_phase_value   = float_t(0.);
    fade_in_phase_value = float_t(0.);
    step_phase_value    = float_t(0.);
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
    float_t resetValue = is_delay_active ? float_t(1.) : float_t(0.);
    for (auto& filter : contour_filters)
    {
        contour_filter::reset(resetValue, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::process(const vector_float& in, vector_float& out)
{
    if (is_delay_active && !delay_phase.update_one_shot(delay_phase_value, 1))
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    auto value_le = channel_steps.at(L).at(step.first);
    auto value_ri = channel_steps.at(ch).at(step.first);

    apply_width(value_le, value_ri, width); //! Width
    apply_contour(value_le, value_ri,
                  contour_filters); //! The filters smooth everything, cracklefree.

    auto tmp_mix = is_fade_in_active ? mix * fade_in_phase_value : mix;
    apply_mix(value_le, value_ri, tmp_mix); //! Mix must be applied last

    out[L] = in[L] * value_le;
    out[R] = in[R] * value_ri;

    update_phases();
}

//------------------------------------------------------------------------
void trance_gate::update_phases()
{
    fade_in_phase.update_one_shot(fade_in_phase_value, 1);
    if (step_phase.update(step_phase_value, 1))
        ++step;
}

//------------------------------------------------------------------------
void trance_gate::set_sample_rate(float_t value)
{
    delay_phase.set_sample_rate(value);
    fade_in_phase.set_sample_rate(value);
    step_phase.set_sample_rate(value);
    for (auto& filter : contour_filters)
    {
        contour_filter::update_sample_rate(value, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_step(i32 channel, i32 step, float_t value)
{
    channel_steps.at(channel).at(step) = value;
}

//------------------------------------------------------------------------
void trance_gate::set_width(float_t value)
{
    width = float_t(1.) - value;
}

//------------------------------------------------------------------------
void trance_gate::set_stereo_mode(bool value)
{
    ch = value ? R : L;
}

//------------------------------------------------------------------------
void trance_gate::set_step_length(float_t value)
{
    step_phase.set_note_length(value);
}
//------------------------------------------------------------------------
void trance_gate::set_tempo(float_t value)
{
    delay_phase.set_tempo(value);
    fade_in_phase.set_tempo(value);
    step_phase.set_tempo(value);
}

//------------------------------------------------------------------------
void trance_gate::set_step_count(i32 value)
{
    step.second = value;
}

//------------------------------------------------------------------------
void trance_gate::set_contour(float_t value)
{
    if (contour == value)
        return;

    contour = value;
    for (auto& filter : contour_filters)
    {
        contour_filter::set_time(value, filter);
    }
}

//------------------------------------------------------------------------
void trance_gate::set_fade_in(float_t value)
{
    is_fade_in_active = value > float_t(0.);
    if (!is_fade_in_active)
        return;

    fade_in_phase.set_note_length(value);
}

//------------------------------------------------------------------------
void trance_gate::set_delay(float_t value)
{
    is_delay_active = value > float_t(0.);
    if (!is_delay_active)
        return;

    delay_phase.set_note_length(value);
}

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha
