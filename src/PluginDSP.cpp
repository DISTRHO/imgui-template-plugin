/*
 * ImGui plugin example
 * Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
 * Copyright (C) 2021-2026 Filipe Coelho <falktx@falktx.com>
 * SPDX-License-Identifier: ISC
 */

#include "DistrhoPlugin.hpp"
#include "Biquad.h"
#include "extra/ScopedDenormalDisable.hpp"
#include "extra/ValueSmoother.hpp"

START_NAMESPACE_DISTRHO

// --------------------------------------------------------------------------------------------------------------------

static constexpr const float db2coef(float g)
{
    return g > -90.f ? std::pow(10.f, g * 0.05f) : 0.f;
}

// --------------------------------------------------------------------------------------------------------------------

class ImGuiPluginDSP : public Plugin
{
    enum {
        kNumBands = 6,
    };

    enum BandParameters {
        kBandParameterEnabled,
        kBandParameterGain,
        kBandParameterFreq,
        kBandParameterQ,
        kBandParameterCount
    };

    enum Parameters {
        kParamBypass,
        kParamReset,
        kParamMainVolume,
        kParamHighPassEnabled,
        kParamHighPassFreq,
        kParamHighPassQ,
        kParamBandsStart,
        kParamBandsEnd = kParamBandsStart + kBandParameterCount * kNumBands - 1,
        kParamLowPassEnabled,
        kParamLowPassFreq,
        kParamLowPassQ,
        kParamCount
    };

    enum ParameterGroups {
        kParameterGroupHighPass,
        kParameterGroupBandsStart,
        kParameterGroupBandsEnd = kParameterGroupBandsStart + kNumBands - 1,
        kParameterGroupLowPass,
    };

    static constexpr const float kParameterDefaults[kParamCount] = {
        // [kParamHighPassFreq] = 20.f,
    };

    struct {
        Biquad highpass;
        Biquad lowpass;
        Biquad bands[kNumBands];
    } filters;

    float fParameters[kParamCount] = {};
    ExponentialValueSmoother fSmoothDryWet;
    ExponentialValueSmoother fSmoothVolume;

public:
   /**
      Plugin class constructor.@n
      You must set all parameter values to their defaults, matching ParameterRanges::def.
    */
    ImGuiPluginDSP()
        : Plugin(kParamCount, 0, 0) // parameters, programs, states
    {
        const double sampleRate = getSampleRate();

        fParameters[kParamHighPassFreq] = 20.f;
        fParameters[kParamHighPassQ] = 0.707f;
        fParameters[kParamLowPassFreq] = 20000.f;
        fParameters[kParamLowPassQ] = 0.707f;

        filters.highpass.setBiquad(bq_type_highpass,
                                   fParameters[kParamHighPassFreq] / sampleRate,
                                   fParameters[kParamHighPassQ],
                                   0.0);

        filters.lowpass.setBiquad(bq_type_lowpass,
                                  fParameters[kParamLowPassFreq] / sampleRate,
                                  fParameters[kParamLowPassQ],
                                  0.0);

        for (uint8_t i = 0; i < kNumBands; ++i)
        {
            const uint32_t start = kParamBandsStart + i * kBandParameterCount;

            // TODO pick from defaults somewhere
            fParameters[start + kBandParameterFreq] = 200.f;
            fParameters[start + kBandParameterQ] = 0.707f;

            filters.bands[i].setBiquad(bq_type_peak,
                                       fParameters[start + kBandParameterFreq] / sampleRate,
                                       fParameters[start + kBandParameterQ],
                                       fParameters[start + kBandParameterGain]);
        }

        fSmoothDryWet.setSampleRate(sampleRate);
        fSmoothDryWet.setTargetValue(1.f);
        fSmoothDryWet.setTimeConstant(0.010f); // 10ms

        fSmoothVolume.setSampleRate(sampleRate);
        fSmoothVolume.setTargetValue(db2coef(0.f));
        fSmoothVolume.setTimeConstant(0.010f); // 10ms
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
        const auto initEnabled = [&parameter]
        {
            parameter.ranges.min = 0.f;
            parameter.ranges.max = 1.f;
            parameter.ranges.def = 0.f;
            parameter.hints = kParameterIsAutomatable | kParameterIsBoolean | kParameterIsInteger;
        };
        const auto initGain = [&parameter]
        {
            parameter.ranges.min = -12.f;
            parameter.ranges.max = 12.f;
            parameter.hints = kParameterIsAutomatable;
            parameter.unit = "dB";
        };
        const auto initFreq = [&parameter]
        {
            parameter.ranges.min = 20.f;
            parameter.ranges.max = 20000.f;
            parameter.hints = kParameterIsAutomatable | kParameterIsLogarithmic;
            parameter.unit = "Hz";
        };
        const auto initQ = [&parameter]
        {
            parameter.ranges.min = 0.5f;
            parameter.ranges.max = 1.f;
            parameter.ranges.def = 0.707f;
            parameter.hints = kParameterIsAutomatable;
        };

        switch (static_cast<Parameters>(index))
        {
        case kParamBypass:
            parameter.initDesignation(kParameterDesignationBypass);
            break;
        case kParamReset:
            parameter.initDesignation(kParameterDesignationReset);
            break;
        case kParamMainVolume:
            parameter.ranges.min = -90.f;
            parameter.ranges.max = 30.f;
            parameter.ranges.def = 0.f;
            parameter.hints = kParameterIsAutomatable;
            parameter.name = "Volume";
            parameter.shortName = "Vol";
            parameter.symbol = "Volume";
            parameter.unit = "dB";
            parameter.enumValues.count = 1;
            parameter.enumValues.values = new ParameterEnumerationValue[1];
            parameter.enumValues.values[0] = { -90.f, "-inf" };
            break;
        case kParamHighPassEnabled:
            initEnabled();
            parameter.groupId = kParameterGroupHighPass;
            parameter.name = "High Pass Enabled";
            parameter.shortName = "HP Enabled";
            parameter.symbol = "HighPassEnabled";
            break;
        case kParamHighPassFreq:
            initFreq();
            parameter.groupId = kParameterGroupHighPass;
            parameter.ranges.def = 20.f;
            parameter.name = "High Pass Freq";
            parameter.shortName = "HP Freq";
            parameter.symbol = "HighPassFreq";
            break;
        case kParamHighPassQ:
            initQ();
            parameter.groupId = kParameterGroupHighPass;
            parameter.name = "High Pass Q";
            parameter.shortName = "HP Q";
            parameter.symbol = "HighPassQ";
            break;
        case kParamBandsStart ... kParamBandsEnd:
        {
            const uint8_t bandIndex = (index - kParamBandsStart) / kBandParameterCount;
            const String bandstr = String(bandIndex + 1);

            parameter.groupId = kParameterGroupBandsStart + bandIndex;

            switch (static_cast<BandParameters>((index - kParamBandsStart) % kBandParameterCount))
            {
            case kBandParameterEnabled:
                initEnabled();
                parameter.name = "Band " + bandstr + " Enabled";
                parameter.shortName = "B" + bandstr + " Enabled";
                parameter.symbol = "B" + bandstr + "Enabled";
                break;
            case kBandParameterGain:
                initGain();
                parameter.name = "Band " + bandstr + " Gain";
                parameter.shortName = "B" + bandstr + " Gain";
                parameter.symbol = "B" + bandstr + "Gain";
                break;
            case kBandParameterFreq:
                initFreq();
                // TODO pick from defaults somewhere
                parameter.ranges.def = 200.f;
                parameter.name = "Band " + bandstr + " Freq";
                parameter.shortName = "B" + bandstr + " Freq";
                parameter.symbol = "B" + bandstr + "Freq";
                break;
            case kBandParameterQ:
                initQ();
                parameter.name = "Band " + bandstr + " Q";
                parameter.shortName = "B" + bandstr + " Q";
                parameter.symbol = "B" + bandstr + "Q";
                break;
            case kBandParameterCount:
                break;
            }
            break;
        }
        case kParamLowPassEnabled:
            initEnabled();
            parameter.groupId = kParameterGroupLowPass;
            parameter.name = "Low Pass Enabled";
            parameter.shortName = "LP Enabled";
            parameter.symbol = "LowPassEnabled";
            break;
        case kParamLowPassFreq:
            initFreq();
            parameter.ranges.def = 20000.f;
            parameter.groupId = kParameterGroupLowPass;
            parameter.name = "Low Pass Freq";
            parameter.shortName = "LP Freq";
            parameter.symbol = "LowPassFreq";
            break;
        case kParamLowPassQ:
            initQ();
            parameter.groupId = kParameterGroupLowPass;
            parameter.name = "Low Pass Q";
            parameter.shortName = "LP Q";
            parameter.symbol = "LowPassQ";
            break;
        case kParamCount:
            break;
        }
    }

    void initPortGroup(uint32_t groupId, PortGroup& portGroup) override
    {
        if (groupId >= 0)
        {
            switch (static_cast<ParameterGroups>(groupId))
            {
            case kParameterGroupHighPass:
                portGroup.name = "High Pass";
                portGroup.symbol = "HighPass";
                break;
            case kParameterGroupBandsStart ... kParameterGroupBandsEnd:
            {
                const String bandstr = String(groupId - kParameterGroupBandsStart + 1);
                portGroup.name = "Band " + bandstr;
                portGroup.symbol = "Band" + bandstr;
                break;
            }
            case kParameterGroupLowPass:
                portGroup.name = "Low Pass";
                portGroup.symbol = "LowPass";
                break;
            }
        }

        Plugin::initPortGroup(groupId, portGroup);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Internal data

   /**
      Get the current value of a parameter.@n
      The host may call this function from any context, including realtime processing.
    */
    float getParameterValue(uint32_t index) const override
    {
        return fParameters[index];
    }

   /**
      Change a parameter value.@n
      The host may call this function from any context, including realtime processing.@n
      When a parameter is marked as automatable, you must ensure no non-realtime operations are performed.
      @note This function will only be called for parameter inputs.
    */
    void setParameterValue(uint32_t index, float value) override
    {
        fParameters[index] = value;

        switch (static_cast<Parameters>(index))
        {
        case kParamBypass:
            fSmoothDryWet.setTargetValue(value > 0.5f ? 0.f : 1.f);
            break;
        case kParamReset:
            reset();
            break;
        case kParamMainVolume:
            fSmoothVolume.setTargetValue(db2coef(std::clamp(value, -90.f, 30.f)));
            break;
        case kParamHighPassEnabled:
            if (value > 0.5f)
                filters.highpass.setFc(std::clamp(fParameters[kParamHighPassFreq] / getSampleRate(), 0.0005, 0.42));
            else
                filters.highpass.setFc(0.0005);
            break;
        case kParamHighPassFreq:
            if (fParameters[kParamHighPassEnabled] > 0.5f)
                filters.highpass.setFc(std::clamp(value / getSampleRate(), 0.0005, 0.42));
            break;
        case kParamHighPassQ:
            filters.highpass.setQ(std::clamp(value, 0.5f, 1.f));
            break;
        case kParamBandsStart ... kParamBandsEnd:
        {
            const uint8_t bandIndex = (index - kParamBandsStart) / kBandParameterCount;
            const uint8_t start = kParamBandsStart + bandIndex * kBandParameterCount;

            Biquad& filter(filters.bands[bandIndex]);

            switch (static_cast<BandParameters>((index - kParamBandsStart) % kBandParameterCount))
            {
            case kBandParameterEnabled:
                if (value > 0.5f)
                    filter.setPeakGain(std::clamp(fParameters[start + kBandParameterGain], -12.f, 12.f));
                else
                    filter.setPeakGain(0.0);
                break;
            case kBandParameterGain:
                if (fParameters[start + kBandParameterEnabled] > 0.5f)
                    filter.setPeakGain(std::clamp(value, -12.f, 12.f));
                break;
            case kBandParameterFreq:
                filter.setFc(std::clamp(value / getSampleRate(), 0.0005, 0.42));
                break;
            case kBandParameterQ:
                filter.setQ(std::clamp(value, 0.5f, 1.f));
                break;
            case kBandParameterCount:
                break;
            }
            break;
        }
        case kParamLowPassEnabled:
            if (value > 0.5f)
                filters.lowpass.setFc(std::clamp(fParameters[kParamLowPassFreq] / getSampleRate(), 0.0005, 0.42));
            else
                filters.lowpass.setFc(0.42);
            break;
        case kParamLowPassFreq:
            if (fParameters[kParamLowPassEnabled] > 0.5f)
                filters.lowpass.setFc(std::clamp(value / getSampleRate(), 0.0005, 0.42));
            break;
        case kParamLowPassQ:
            filters.lowpass.setQ(std::clamp(value, 0.5f, 1.f));
            break;
        case kParamCount:
            break;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Audio/MIDI Processing

    void reset()
    {
        fSmoothDryWet.clearToTargetValue();
        fSmoothVolume.clearToTargetValue();
    }

   /**
      Activate this plugin.
    */
    void activate() override
    {
        reset();
    }

   /**
      Run/process function for plugins without MIDI input.
      @note Some parameters might be null if there are no audio inputs or outputs.
    */
    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const ScopedDenormalDisable sdd;

        // get the left and right audio inputs
        const float* const inpL = inputs[0];
        // const float* const inpR = inputs[1];

        // get the left and right audio outputs
        float* const outL = outputs[0];
        // float* const outR = outputs[1];

        // apply gain against all samples
        float flt, vol, wet;
        for (uint32_t i=0; i < frames; ++i)
        {
            wet = fSmoothDryWet.next();
            vol = fSmoothVolume.next();

            flt = filters.lowpass.process(inpL[i]);
            for (uint8_t i = 0; i < kNumBands; ++i)
                flt = filters.bands[i].process(flt);
            flt = filters.highpass.process(flt) * vol;

            outL[i] = flt * wet + inpL[i] * (1.f - wet);
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Callbacks (optional)

   /**
      Optional callback to inform the plugin about a sample rate change.@n
      This function will only be called when the plugin is deactivated.
      @see getSampleRate()
    */
    void sampleRateChanged(const double newSampleRate) override
    {
        filters.lowpass.setFc(std::clamp(fParameters[kParamLowPassFreq] / newSampleRate, 0.0005, 0.42));
        filters.highpass.setFc(std::clamp(fParameters[kParamHighPassFreq] / newSampleRate, 0.0005, 0.42));

        for (uint8_t i = 0; i < kNumBands; ++i)
        {
            const uint8_t start = kParamBandsStart + i * kBandParameterCount;
            filters.bands[i].setFc(std::clamp(fParameters[start + kBandParameterFreq] / newSampleRate, 0.0005, 0.42));
        }

        fSmoothVolume.setSampleRate(newSampleRate);
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
