// Copyright(c) 2016 René Hansen.

#pragma once

#include "ha/dsp_tool_box/filtering/one_pole.h"
#include "ha/dsp_tool_box/modulation/modulation_phase.h"
#include "ha/fx_collection/types.h"
#include <array>
#include <vector>

namespace ha::fx_collection {

//------------------------------------------------------------------------
/**
 * trance_gate
 */

struct TranceGate
{
    static constexpr i32 NUM_CHANNELS  = 2;
    static constexpr i32 MIN_NUM_STEPS = 1;
    static constexpr i32 MAX_NUM_STEPS = 32;
    static constexpr i32 L             = 0;
    static constexpr i32 R             = 1;

    using StepValues     = std::array<mut_f32, MAX_NUM_STEPS>;
    using ChannelSteps   = std::array<StepValues, NUM_CHANNELS>;
    using ContourFilters = std::array<dtb::filtering::OnePole, NUM_CHANNELS>;
    alignas(BYTE_ALIGNMENT) ChannelSteps channel_steps;
    ContourFilters contour_filters;

    struct Step
    {
        mut_i32 pos     = 0;
        mut_i32 count   = 16;
        bool is_shuffle = false;
    };

    /*
        Wir haben 3 Phasen, die hier zum Einsatz kommen:
        1. Delay Phase: wenn sie "1" erreicht, läuft das TranceGate los
        2. Fade In Phase: Dauer der TranceGate fade in Zeit
        3. Step Phase: läuft über die Dauer eines einzelnen Steps
    */
    dtb::modulation::Phase delay_phase;
    dtb::modulation::Phase fade_in_phase;
    dtb::modulation::Phase step_phase;
    mut_f32 delay_phase_val   = f32(0.);
    mut_f32 step_phase_val    = f32(0.);
    mut_f32 fade_in_phase_val = f32(0.);

    Step step_val;
    mut_f32 mix            = f32(1.);
    mut_f32 width          = f32(0.);
    mut_f32 shuffle        = f32(0.);
    mut_f32 contour        = f32(0.01);
    mut_f32 sample_rate    = f32(44100.);
    mut_i32 ch             = L;
    bool is_delay_active   = false;
    bool is_fade_in_active = false;
};

struct TranceGateImpl final
{
    /**
     * @brief Initialises the trance gate.
     */
    static TranceGate create();

    /**
     * @brief Processes one audio frame (4 channels).
     */
    static void
    process(TranceGate& trance_gate, AudioFrame const& in, AudioFrame& out);

    /**
     * @brief Sets the sample rate in [Hz].
     */
    static void set_sample_rate(TranceGate& trance_gate, f32 value);

    /**
     * @brief Sets the tempo in [BPM].
     */
    static void set_tempo(TranceGate& trance_gate, f32 value);

    /**
     * @brief Updates the musical project time [quarter notes].
     */
    static void update_project_time_music(TranceGate& trance_gate, f64 value);

    /**
     * @brief Triggers the trance gate.
     *
     * Both delay_len and fade_in_len can only be set when triggering the gate.
     * Changing these values during the gate is running makes no sense.
     *
     * @param delay_len Length of delay time in [seconds] until the trance gate
     * starts processing.
     * @param fade_in_len Length of fade_in time in [seconds].
     */
    static void trigger(TranceGate& trance_gate,
                        f32 delay_len   = f32(0.),
                        f32 fade_in_len = f32(0.));

    /**
     * @brief Resets filters and values.
     */
    static void reset(TranceGate& trance_gate);

    /**
     * @brief Resets the current step position.
     */
    static void reset_step_pos(TranceGate& trance_gate, i32 value);

    /**
     * @brief Returns the current step position.
     */
    static i32 get_step_pos(const TranceGate& trance_gate);

    /**
     * @brief Sets the pattern length in steps.
     * @param value Number of steps until repeat
     */
    static void set_step_count(TranceGate& trance_gate, i32 value);

    /**
     * @brief Sets the amount of a step.
     * @param channel The channel index of the step to change
     * @param step The index of the step to change
     * @param value Defining the amount [normalised] of a step, can also be just
     * on or off
     */
    static void set_step(TranceGate& trance_gate,
                         i32 channel,
                         i32 step,
                         f32 value_normalised);

    /**
     * @brief Sets the note length of a step. e.g. for 1/32th length, pass in
     * 0.03125
     * @param value [1/128 - 1] (Normal, Triolic, Dotted) Defining the length of
     * a step
     */
    static void set_step_len(TranceGate& trance_gate, f32 value_note_len);

    /**
     * @brief Sets the mix of the trance gate.
     * @param value Defining the amount [normalised] of the trance gate
     */
    static void set_mix(TranceGate& trance_gate, f32 value)
    {
        trance_gate.mix = value;
    }

    /**
     * @brief Sets the contour of the trance gate.
     * @param value Defining duration in [seconds: 4s - 0.001s] of attack and
     * release slope
     */
    static void set_contour(TranceGate& trance_gate, f32 value_seconds);

    /**
     * @brief Set the gate to either mono or stereo.
     */
    static void set_stereo_mode(TranceGate& trance_gate, bool value);

    /**
     * @brief Sets the width of the trance gate.
     * @param value Defining the amount [normalised] of stereo effect
     */
    static void set_width(TranceGate& trance_gate, f32 value_normalised);

    /**
     * @brief Sets the amount of shuffle for the trance gate.
     * @param value Defining the amount [normalised] of shuffle
     */
    static void set_shuffle_amount(TranceGate& trance_gate, f32 value);

private:
    static void set_fade_in(TranceGate& trance_gate, f32 value);
    static void set_delay(TranceGate& trance_gate, f32 value);
    static void update_phases(TranceGate& trance_gate);
};

//------------------------------------------------------------------------
} // namespace ha::fx_collection
