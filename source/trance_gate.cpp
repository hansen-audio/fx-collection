// Copyright(c) 2016 René Hansen.

#include "ha/fx_collection/trance_gate.h"

namespace ha {
namespace fx_collection {

//------------------------------------------------------------------------
static void applyWidth(float_t& valueL, float_t& valueR, float_t width)
{
    valueL = std::max(valueL, valueR * width);
    valueR = std::max(valueR, valueL * width);
}

//------------------------------------------------------------------------
static void applyMix(float_t& value, float_t mix)
{
    static const float_t kMixMax = 1.f;
    value                        = (kMixMax - mix) + value * mix;
}

//------------------------------------------------------------------------
static void applyMix(float_t& valueL, float_t& valueR, float_t mix)
{
    applyMix(valueL, mix);
    applyMix(valueR, mix);
}

//------------------------------------------------------------------------
static void applyContour(float_t& valueL, float_t& valueR, TranceGate::ContourFilters& contour)
{
    valueL = ContourFilter::process(valueL, contour.at(TranceGate::L));
    valueR = ContourFilter::process(valueR, contour.at(TranceGate::R));
}

//------------------------------------------------------------------------
static void operator++(TranceGate::StepPos& step)
{
    ++step.first;
    if (!(step.first < step.second))
        step.first = 0;
}

//------------------------------------------------------------------------
//	TranceGate
//------------------------------------------------------------------------
TranceGate::TranceGate()
{
    contourFilters.resize(kNumChannels);
    channelSteps.resize(kNumChannels);
    for (auto& step : channelSteps)
        step.resize(kNumSteps, 0.f);

    delayPhase.set_rate(ha::dtb::modulation::phase::note_length_to_rate(1.f / 32.f));
    delayPhase.set_tempo(120.f);
    delayPhase.set_mode(ha::dtb::modulation::phase::MODE_TEMPO_SYNC);

    fadeInPhase.set_rate(ha::dtb::modulation::phase::note_length_to_rate(1.f / 32.f));
    fadeInPhase.set_tempo(120.f);
    fadeInPhase.set_mode(ha::dtb::modulation::phase::MODE_TEMPO_SYNC);

    stepPhase.set_rate(ha::dtb::modulation::phase::note_length_to_rate(1.f / 32.f));
    stepPhase.set_tempo(120.f);
    stepPhase.set_mode(ha::dtb::modulation::phase::MODE_TEMPO_SYNC);
}

//------------------------------------------------------------------------
void TranceGate::trigger(float_t delayLength, float_t fadeInLength)
{
    //! Delay and FadeIn time can ony be set in trigger. Changing
    //! these parameters after trigger resp. during the gate is
    //! running makes no sense.
    setDelay(delayLength);
    setFadeIn(fadeInLength);

    delayPhase.reset_one_shot();
    fadeInPhase.reset_one_shot();
    stepPhase.reset();
    step.first = 0;

    /*	Do not reset filters in trigger. Because there can still be a voice
        in release playing back. Dann bricht auf einmal Audio weg wenn wir
        hier die Filer auf null setzen. Das klingt komisch dann. Also muss
        das von draussen gesteuert werden, dass wenn Stille herrscht, die Filter
        genullt bzw zurückgesetzt werden ([reset] Methode aufrufen).

        Es gibt eine Ausnahme, nämlich wenn das TG delay an ist. Dann werden
        die Filter auf 1.0 resetted und das muss auch im ::trigger passieren.
    */

    if (isDelayActive)
        reset();
}

//------------------------------------------------------------------------
void TranceGate::reset()
{
    /*  Wenn Delay aktiv ist müssen die Filter auf 1.f resetted werden.
        Ansonsten klickt das leicht, weil die Filter nach dem Delay
        erst ganz kurz von 0.f - 1.f einschwingen müssen. Das hört man in Form
        eines Klickens.
    */
    float_t resetValue = isDelayActive ? 1.f : 0.f;
    for (auto& filter : contourFilters)
    {
        ContourFilter::reset(resetValue, filter);
    }
}

//------------------------------------------------------------------------
void TranceGate::process(const float_vector& in, float_vector& out)
{
    if (isDelayActive && !delayPhase.update_one_shot(1))
    {
        out = in;
        return;
    }

    //! Get the step value. Right channel depends on Stereo mode
    auto valueL = channelSteps.at(L).at(step.first);
    auto valueR = channelSteps.at(ch).at(step.first);

    applyWidth(valueL, valueR, width);            //! Width
    applyContour(valueL, valueR, contourFilters); //! The filters smooth everything, cracklefree.

    auto tmpMix = isFadeInActive ? mix * fadeInPhase.get_one_shot_phase() : mix;
    applyMix(valueL, valueR, tmpMix); //! Mix must be applied last

    out[L] = in[L] * valueL;
    out[R] = in[R] * valueR;

    updatePhases();
}

//------------------------------------------------------------------------
void TranceGate::updatePhases()
{
    fadeInPhase.update_one_shot(1);
    if (stepPhase.update(1))
        ++step;
}

//------------------------------------------------------------------------
void TranceGate::setSamplerate(float_t value)
{
    delayPhase.set_sample_rate(value);
    fadeInPhase.set_sample_rate(value);
    stepPhase.set_sample_rate(value);
    for (auto& filter : contourFilters)
    {
        ContourFilter::updateSamplerate(value, filter);
    }
}

//------------------------------------------------------------------------
void TranceGate::setStep(int32_t channel, int32_t step, float_t value)
{
    channelSteps.at(channel).at(step) = value;
}

//------------------------------------------------------------------------
void TranceGate::setWidth(float_t value)
{
    width = 1.f - value;
}

//------------------------------------------------------------------------
void TranceGate::setStereoMode(bool value)
{
    ch = value ? R : L;
}

//------------------------------------------------------------------------
void TranceGate::setStepLength(float_t value)
{
    stepPhase.set_note_length(value);
}
//------------------------------------------------------------------------
void TranceGate::setTempo(float_t value)
{
    delayPhase.set_tempo(value);
    fadeInPhase.set_tempo(value);
    stepPhase.set_tempo(value);
}

//------------------------------------------------------------------------
void TranceGate::setStepCount(int32_t value)
{
    step.second = value;
}

//------------------------------------------------------------------------
void TranceGate::setContour(float_t value)
{
    if (contour == value)
        return;

    contour = value;
    for (auto& filter : contourFilters)
    {
        ContourFilter::setTime(value, filter);
    }
}

//------------------------------------------------------------------------
void TranceGate::setFadeIn(float_t value)
{
    isFadeInActive = value > 0.f;
    if (!isFadeInActive)
        return;

    fadeInPhase.set_note_length(value);
}

//------------------------------------------------------------------------
void TranceGate::setDelay(float_t value)
{
    isDelayActive = value > 0.f;
    if (!isDelayActive)
        return;

    delayPhase.set_note_length(value);
}

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha
