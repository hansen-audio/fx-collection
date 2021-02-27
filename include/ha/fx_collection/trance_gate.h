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
class smoother final
{
public:
    //--------------------------------------------------------------------
    struct context_data
    {
        Real a = 0;
        Real b = 0;
        Real z = 0;
    };

    static context_data init(Real a = 0.9)
    {
        context_data data{a, Real(1.) - a, 0};

        return data;
    }

    static void updatePol(Real a, context_data& data)
    {
        data.a = a;
        data.b = Real(1.) - data.a;
    }

    static Real smooth(Real in, context_data& data)
    {
        if (is_equal(in, data))
            return data.z;

        data.z = (in * data.b) + (data.z * data.a);
        return data.z;
    }

    static void reset(Real in, context_data& data) { data.z = in; }
    //--------------------------------------------------------------------
private:
    static bool is_equal(Real in, const context_data& data) { return in == data.z; }
};

//------------------------------------------------------------------------
//	contour_filter
//	TODO: Bei Gelegenheit nochmal Generaisieren den contour_filter.
//------------------------------------------------------------------------
class contour_filter
{
public:
    //--------------------------------------------------------------------
    struct context_data
    {
        float_t sample_rate = 1.f;
        smoother::context_data smoother_data;
    };

    static void update_sample_rate(float_t value, context_data& data) { data.sample_rate = value; }
    static void reset(float_t value, context_data& data)
    {
        smoother::reset(value, data.smoother_data);
    }
    static float_t process(float_t value, context_data& data)
    {
        return smoother::smooth(value, data.smoother_data);
    }

    // @param value Given in seconds.
    static void set_time(float_t value, context_data& data)
    {
        auto pole = tau_to_pole(value, data.sample_rate);
        smoother::updatePol(pole, data.smoother_data);
    }
    //--------------------------------------------------------------------
private:
    static float_t tau_to_pole(float_t tau, float_t sample_rate)
    {
        //! https://en.wikipedia.org/wiki/Time_constant
        //! (5 * Tau) means 99.3% reached thats sufficient.
        constexpr float_t RECIPROCAL_5 = float(1. / 5.);
        return exp(float_t(-1) / ((tau * RECIPROCAL_5) * sample_rate));
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
    using contour_filters_list = std::vector<contour_filter::context_data>;
    using step_pos             = std::pair<i32, i32>;

    trance_gate();

    void process(const float_vector& in, float_vector& out);
    void set_sample_rate(float_t value);
    void set_tempo(float_t value);
    void trigger(float_t delayLength = float_t(0.), float_t withFadeIn = float_t(0.));
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
    float_t mix     = float_t(1.);
    float_t width   = float_t(0.);
    i32 ch          = L;
    float_t contour = float_t(-1.);

    bool is_delay_active   = false;
    bool is_fade_in_active = false;
};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha