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
//	contour_filter
//	TODO: Bei Gelegenheit nochmal Generaisieren den contour_filter.
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
/**	trance_gate */
//------------------------------------------------------------------------
class trance_gate
{
public:
    //--------------------------------------------------------------------
    static constexpr i32 NUM_CHANNELS = 2;
    static constexpr i32 NUM_STEPS    = 32;
    static constexpr i32 L            = 0;
    static constexpr i32 R            = 1;

    using step_value           = std::vector<float_t>;
    using channel_steps_list   = std::vector<step_value>;
    using contour_filters_list = std::vector<ContourFilter::Data>;
    using step_pos             = std::pair<i32, i32>;

    trance_gate();

    void process(const float_vector& in, float_vector& out);
    void set_sample_rate(float_t value);
    void set_tempo(float_t value);
    void trigger(float_t delayLength = 0.f, float_t withFadeIn = 0.f);
    void reset();

    void set_step_count(i32 value);
    void set_step(i32 channel, i32 step, float_t value);
    void set_step_length(float_t value);
    void set_mix(float_t value) { mix = value; }
    void set_contour(float_t value);
    void set_stereo_mode(bool value);
    void set_width(float_t value);

    //--------------------------------------------------------------------
private:
    void set_fade_in(float_t value);
    void set_delay(float_t value);
    void update_phases();

    channel_steps_list channel_steps;
    contour_filters_list contour_filters;

    /*
        Wir haben 3 Phasen, die hier zum Einsatz kommen:
        1. Delay Phase: wenn sie "1" erreicht, läuft das TranceGate los
        2. Fade In Phase: Dauer der TranceGate fade in Zeit
        3. Step Phase: läuft über die Dauer eines einzelnen Steps
    */
    ha::dtb::modulation::one_shot_phase delay_phase;
    ha::dtb::modulation::one_shot_phase fade_in_phase;
    ha::dtb::modulation::phase step_phase;

    step_pos step;
    float_t mix     = 1.f;
    float_t width   = 0.f;
    i32 ch          = L;
    float_t contour = -1.f;

    bool is_delay_active   = false;
    bool is_fade_in_active = false;
};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha