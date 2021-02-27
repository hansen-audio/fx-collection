// Copyright(c) 2016 René Hansen.

#pragma once

#include "ha/dsp_tool_box/modulation/adsr_envelope.h"
#include "ha/dsp_tool_box/modulation/modulation_phase.h"
#include "ha/fx_collection/types.h"
#include <vector>

namespace ha {
namespace fx_collection {

using Real = float;

//------------------------------------------------------------------------
/** A smoothing-filter like dezipper. Actually a one-pole filter. */
//------------------------------------------------------------------------
class Smoother
{
public:
    //--------------------------------------------------------------------
    struct Data
    {
        Real a = 0;
        Real b = 0;
        Real z = 0;
    };

    static Data init(Real a = 0.9)
    {
        Data data{a, Real(1.) - a, 0};

        return data;
    }

    static void updatePol(Real a, Data& data)
    {
        data.a = a;
        data.b = Real(1.) - data.a;
    }

    static Real smooth(Real in, Data& data)
    {
        if (isEqual(in, data))
            return data.z;

        data.z = (in * data.b) + (data.z * data.a);
        return data.z;
    }

    static void reset(Real in, Data& data) { data.z = in; }
    //--------------------------------------------------------------------
private:
    static bool isEqual(Real in, const Data& data) { return in == data.z; }
};

//------------------------------------------------------------------------
//	ContourFilter
//	TODO: Bei Gelegenheit nochmal Generaisieren den ContourFilter.
//------------------------------------------------------------------------
class ContourFilter
{
public:
    //--------------------------------------------------------------------
    struct Data
    {
        float_t samplerate = 1.f;
        Smoother::Data smootherData;
    };

    static void updateSamplerate(float_t value, Data& data) { data.samplerate = value; }
    static void reset(float_t value, Data& data) { Smoother::reset(value, data.smootherData); }
    static float_t process(float_t value, Data& data)
    {
        return Smoother::smooth(value, data.smootherData);
    }

    // @param value Given in seconds.
    static void setTime(float_t value, Data& data)
    {
        auto pole = tau2pole(value, data.samplerate);
        Smoother::updatePol(pole, data.smootherData);
    }
    //--------------------------------------------------------------------
private:
    static float_t tau2pole(float_t tau, float_t sr)
    {
        //! https://en.wikipedia.org/wiki/Time_constant
        //! (5 * Tau) means 99.3% reached thats sufficient.
        constexpr float_t kFiveRecip = 1.f / 5.f;
        return exp(-1 / ((tau * kFiveRecip) * sr));
    }
};

//------------------------------------------------------------------------
/**	TranceGate */
//------------------------------------------------------------------------
class TranceGate
{
public:
    //--------------------------------------------------------------------
    static constexpr i32 kNumChannels = 2;
    static constexpr i32 kNumSteps    = 32;
    static constexpr i32 L            = 0;
    static constexpr i32 R            = 1;

    using StepValue      = std::vector<float_t>;
    using ChannelSteps   = std::vector<StepValue>;
    using ContourFilters = std::vector<ContourFilter::Data>;
    using StepPos        = std::pair<int32_t, int32_t>;

    TranceGate();

    void process(const float_vector& in, float_vector& out);
    void setSamplerate(float_t value);
    void setTempo(float_t value);
    void trigger(float_t delayLength = 0.f, float_t withFadeIn = 0.f);
    void reset();

    void setStepCount(int32_t value);
    void setStep(int32_t channel, int32_t step, float_t value);
    void setStepLength(float_t value);
    void setMix(float_t value) { mix = value; }
    void setContour(float_t value);
    void setStereoMode(bool value);
    void setWidth(float_t value);

    //--------------------------------------------------------------------
private:
    void setFadeIn(float_t value);
    void setDelay(float_t value);
    void updatePhases();

    ChannelSteps channelSteps;
    ContourFilters contourFilters;

    /*
        Wir haben 3 Phasen, die hier zum Einsatz kommen:
        1. Delay Phase: wenn sie "1" erreicht, läuft das TranceGate los
        2. Fade In Phase: Dauer der TranceGate fade in Zeit
        3. Step Phase: läuft über die Dauer eines einzelnen Steps
    */
    ha::dtb::modulation::one_shot_phase delayPhase;
    ha::dtb::modulation::one_shot_phase fadeInPhase;
    ha::dtb::modulation::phase stepPhase;

    StepPos step;
    float_t mix     = 1.f;
    float_t width   = 0.f;
    int32_t ch      = L;
    float_t contour = -1.f;

    bool isDelayActive  = false;
    bool isFadeInActive = false;
};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha