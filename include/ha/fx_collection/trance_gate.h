// Copyright(c) 2016 René Hansen.

#pragma once

#include "ha/dsp_tool_box/filtering/one_pole.h"
#include "ha/dsp_tool_box/modulation/adsr_envelope.h"
#include "ha/dsp_tool_box/modulation/modulation_phase.h"
#include "ha/fx_collection/types.h"
#include <vector>

namespace ha {
namespace fx_collection {

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
    using contour_filters_list = std::vector<ha::dtb::filtering::one_pole_filter::context_data>;
    using step_pos             = std::pair<i32, i32>; // current_step, step_count

    trance_gate();

    void process(const vector_float& in, vector_float& out);
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
    float_t delay_phase_value   = float_t(0.);
    float_t step_phase_value    = float_t(0.);
    float_t fade_in_phase_value = float_t(0.);

    step_pos step;
    float_t mix            = float_t(1.);
    float_t width          = float_t(0.);
    i32 ch                 = L;
    float_t contour        = float_t(-1.);
    float_t sample_rate    = float_t(1.);
    bool is_delay_active   = false;
    bool is_fade_in_active = false;
};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha