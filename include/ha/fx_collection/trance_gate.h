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
/**
 * trance_gate
 */
class trance_gate
{
public:
    //--------------------------------------------------------------------
    static constexpr i32 NUM_CHANNELS  = 2;
    static constexpr i32 MIN_NUM_STEPS = 1;
    static constexpr i32 MAX_NUM_STEPS = 32;
    static constexpr i32 L             = 0;
    static constexpr i32 R             = 1;

    using step_values          = std::vector<mut_real>;
    using channel_steps_list   = std::vector<step_values>;
    using contour_filters_list = std::vector<dtb::filtering::one_pole_filter::context_data>;
    using step_pos             = std::pair<mut_i32, mut_i32>; // current_step, step_count

    trance_gate();

    /**
     * @brief Processes one audio frame (4 channels).
     */
    void process(audio_frame_t const& in, audio_frame_t& out);

    /**
     * @brief Sets the sample rate in Hz.
     */
    void set_sample_rate(real value);

    /**
     * @brief Sets the tempo in BPM.
     */
    void set_tempo(real value);

    /**
     * @brief Triggers the trance gate.
     *
     * Both delay_len and fade_in_len can only be set when triggering the gate.
     * Changing these values during the gate is running makes no sense.
     *
     * @param delay_len Length of delay time until the trance gate starts processing.
     * @param fade_in_len Length of fade_in time.
     */
    void trigger(real delay_len = real(0.), real fade_in_len = real(0.));

    /**
     * @brief Resets filters and values.
     */
    void reset();

    /**
     * @brief Sets the pattern length in steps.
     * @param value Number of steps until repeat
     */
    void set_step_count(i32 value);

    /**
     * @brief Sets the amount of a step.
     * @param channel The channel index of the step to change
     * @param step The index of the step to change
     * @param value [0.0 - 1.0] Defining the amount of a step, can also be just on or off
     */
    void set_step(i32 channel, i32 step, real value_normalised);

    /**
     * @brief Sets the note length of a step. e.g. for 1/32th length, pass in 0.03125
     * @param value [1/128 - 1] (Normal, Triolic, Dotted) Defining the length of a step
     */
    void set_step_length(real value_note_length);

    /**
     * @brief Sets the mix of the trance gate.
     * @param value [0.0 - 1.0] Defining the amount of the trance gate
     */
    void set_mix(real value) { mix = value; }

    /**
     * @brief Sets the contour of the trance gate.
     * @param value [4s - 0.001s] Defining duration of attack and release slope
     */
    void set_contour(real value_seconds);

    void set_stereo_mode(bool value);

    /**
     * @brief Sets the width of the trance gate.
     * @param value [0.0 - 1.0] Defining the amount of stereo effect
     */
    void set_width(real value_normalised);

    //--------------------------------------------------------------------
private:
    void set_fade_in(real value);
    void set_delay(real value);
    void update_phases();

    channel_steps_list channel_steps;
    contour_filters_list contour_filters;

    /*
        Wir haben 3 Phasen, die hier zum Einsatz kommen:
        1. Delay Phase: wenn sie "1" erreicht, läuft das TranceGate los
        2. Fade In Phase: Dauer der TranceGate fade in Zeit
        3. Step Phase: läuft über die Dauer eines einzelnen Steps
    */
    dtb::modulation::one_shot_phase::context_t delay_phase_ctx;
    dtb::modulation::one_shot_phase::context_t fade_in_phase_ctx;
    dtb::modulation::phase::context_t step_phase_ctx;
    mut_real delay_phase_val   = real(0.);
    mut_real step_phase_val    = real(0.);
    mut_real fade_in_phase_val = real(0.);

    step_pos step_pos_val;
    mut_real mix           = real(1.);
    mut_real width         = real(0.);
    mut_real contour       = real(-1.);
    mut_real sample_rate   = real(1.);
    mut_i32 ch             = L;
    bool is_delay_active   = false;
    bool is_fade_in_active = false;
};

//------------------------------------------------------------------------
} // namespace fx_collection
} // namespace ha