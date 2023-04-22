/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2023 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoPlugin.hpp"
#include "extra/ValueSmoother.hpp"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

static constexpr const float CLAMP(float v, float min, float max)
{
    return std::min(max, std::max(min, v));
}

static constexpr const float DB_CO(float g)
{
    return g > -90.f ? std::pow(10.f, g * 0.05f) : 0.f;
}

// --------------------------------------------------------------------------------------------------------------------

class ImGuiPluginDSP : public Plugin
{
    enum Parameters {
        kParamGain = 0,
        kParamCount
    };

    float fGainDB = 0.0f;
    ExponentialValueSmoother fSmoothGain;

public:
   /**
      Plugin class constructor.@n
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ImGuiPluginDSP()
        : Plugin(kParamCount, 0, 0) // parameters, programs, states
    {
        fSmoothGain.setSampleRate(getSampleRate());
        fSmoothGain.setTargetValue(DB_CO(0.f));
        fSmoothGain.setTimeConstant(0.020f); // 20ms
    }

protected:
    // ----------------------------------------------------------------------------------------------------------------
    // Information

   /**
      Get the plugin label.@n
      This label is a short restricted name consisting of only _, a-z, A-Z and 0-9 characters.
    */
    const char* getLabel() const noexcept override
    {
        return "SimpleGain";
    }

   /**
      Get an extensive comment/description about the plugin.@n
      Optional, returns nothing by default.
    */
    const char* getDescription() const override
    {
        return "A simple audio volume gain plugin with ImGui for its GUI";
    }

   /**
      Get the plugin author/maker.
    */
    const char* getMaker() const noexcept override
    {
        return "Jean Pierre Cimalando, falkTX";
    }

   /**
      Get the plugin license (a single line of text or a URL).@n
      For commercial plugins this should return some short copyright information.
    */
    const char* getLicense() const noexcept override
    {
        return "ISC";
    }

   /**
      Get the plugin version, in hexadecimal.
      @see d_version()
    */
    uint32_t getVersion() const noexcept override
    {
        return d_version(1, 0, 0);
    }

   /**
      Get the plugin unique Id.@n
      This value is used by LADSPA, DSSI and VST plugin formats.
      @see d_cconst()
    */
    int64_t getUniqueId() const noexcept override
    {
        return d_cconst('d', 'I', 'm', 'G');
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Init

   /**
      Initialize the parameter @a index.@n
      This function will be called once, shortly after the plugin is created.
    */
    void initParameter(uint32_t index, Parameter& parameter) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(index == 0,);

        parameter.ranges.min = -90.0f;
        parameter.ranges.max = 30.0f;
        parameter.ranges.def = 0.0f;
        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Gain";
        parameter.shortName = "Gain";
        parameter.symbol = "gain";
        parameter.unit = "dB";
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Internal data

   /**
      Get the current value of a parameter.@n
      The host may call this function from any context, including realtime processing.
    */
    float getParameterValue(uint32_t index) const override
    {
        DISTRHO_SAFE_ASSERT_RETURN(index == 0, 0.0f);

        return fGainDB;
    }

   /**
      Change a parameter value.@n
      The host may call this function from any context, including realtime processing.@n
      When a parameter is marked as automatable, you must ensure no non-realtime operations are performed.
      @note This function will only be called for parameter inputs.
    */
    void setParameterValue(uint32_t index, float value) override
    {
        DISTRHO_SAFE_ASSERT_RETURN(index == 0,);

        fGainDB = value;
        fSmoothGain.setTargetValue(DB_CO(CLAMP(value, -90.0, 30.0)));
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

   /**
      Activate this plugin.
    */
    void activate() override
    {
        fSmoothGain.clearToTargetValue();
    }

   /**
      Run/process function for plugins without MIDI input.
      @note Some parameters might be null if there are no audio inputs or outputs.
    */
    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // get the left and right audio inputs
        const float* const inpL = inputs[0];
        const float* const inpR = inputs[1];

        // get the left and right audio outputs
        float* const outL = outputs[0];
        float* const outR = outputs[1];

        // apply gain against all samples
        for (uint32_t i=0; i < frames; ++i)
        {
            const float gain = fSmoothGain.next();
            outL[i] = inpL[i] * gain;
            outR[i] = inpR[i] * gain;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Callbacks (optional)

   /**
      Optional callback to inform the plugin about a sample rate change.@n
      This function will only be called when the plugin is deactivated.
      @see getSampleRate()
    */
    void sampleRateChanged(double newSampleRate) override
    {
        fSmoothGain.setSampleRate(newSampleRate);
    }

    // ----------------------------------------------------------------------------------------------------------------

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImGuiPluginDSP)
};

// --------------------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new ImGuiPluginDSP();
}

// --------------------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
