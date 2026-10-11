#include "Presets.h"
#include "Parameters.h"

namespace onirique
{

namespace
{
    using V = std::vector<std::pair<juce::String, float>>;

    // Raccourcis : construisent un identifiant complet "<module>_<suffixe>".
    juce::String p (const juce::String& module, const juce::String& suffix) { return module + "_" + suffix; }

    FactoryPreset cleanVocal()
    {
        using namespace ModuleIDs;
        return {
            "Clean Vocal",
            V {
                { p (noiseGate, "threshold"), -50.0f },
                { p (noiseGate, "hysteresis"), 4.0f },

                { p (sslChannel, "hpf"), 80.0f },
                { p (sslChannel, "threshold"), -16.0f },
                { p (sslChannel, "ratio"), 2.5f },
                { p (sslChannel, "attack"), 15.0f },
                { p (sslChannel, "release"), 150.0f },
                { p (sslChannel, "makeup"), 1.5f },

                { p (deEsser1, "threshold"), -22.0f },
                { p (deEsser1, "range"), 6.0f },

                { p (saturation, "bypass"), 1.0f },
                { p (fairchild670, "bypass"), 1.0f },

                { p (eq, "band7_gain"), 1.5f },
            }
        };
    }

    FactoryPreset rapVocal()
    {
        using namespace ModuleIDs;
        return {
            "Rap Vocal",
            V {
                { p (noiseGate, "threshold"), -42.0f },
                { p (noiseGate, "hold"), 40.0f },

                { p (sslChannel, "hpf"), 90.0f },
                { p (sslChannel, "lowMidGain"), -1.5f },
                { p (sslChannel, "highMidGain"), 1.5f },
                { p (sslChannel, "threshold"), -14.0f },
                { p (sslChannel, "ratio"), 4.0f },
                { p (sslChannel, "attack"), 8.0f },
                { p (sslChannel, "release"), 90.0f },
                { p (sslChannel, "makeup"), 2.0f },

                { p (deEsser1, "threshold"), -24.0f },
                { p (deEsser1, "range"), 10.0f },

                { p (saturation, "drive"), 8.0f },
                { p (saturation, "type"), 2.0f },
                { p (saturation, "tone"), 0.1f },
                { p (saturation, "mix"), 35.0f },

                { p (fet1176, "threshold"), -20.0f },
                { p (fet1176, "ratio"), 1.0f },
                { p (fet1176, "attack"), 0.1f },
                { p (fet1176, "release"), 150.0f },
                { p (fet1176, "mix"), 80.0f },

                { p (fairchild670, "bypass"), 1.0f },

                { p (eq, "band1_freq"), 100.0f },
                { p (eq, "band7_gain"), 2.5f },
                { p (eq, "band8_freq"), 15000.0f },
            }
        };
    }

    FactoryPreset aggressiveVocal()
    {
        using namespace ModuleIDs;
        return {
            "Aggressive Vocal",
            V {
                { p (noiseGate, "threshold"), -38.0f },
                { p (noiseGate, "hold"), 20.0f },
                { p (noiseGate, "release"), 80.0f },

                { p (sslChannel, "threshold"), -18.0f },
                { p (sslChannel, "ratio"), 6.0f },
                { p (sslChannel, "attack"), 3.0f },
                { p (sslChannel, "release"), 60.0f },
                { p (sslChannel, "makeup"), 3.0f },

                { p (deEsser1, "threshold"), -26.0f },
                { p (deEsser1, "range"), 14.0f },

                { p (saturation, "drive"), 14.0f },
                { p (saturation, "type"), 1.0f },
                { p (saturation, "mix"), 55.0f },

                { p (fet1176, "threshold"), -24.0f },
                { p (fet1176, "ratio"), 3.0f },
                { p (fet1176, "attack"), 0.02f },
                { p (fet1176, "release"), 60.0f },
                { p (fet1176, "mix"), 100.0f },
                { p (fet1176, "autoGain"), 1.0f },

                { p (fairchild670, "threshold"), -16.0f },
                { p (fairchild670, "ratio"), 4.0f },
                { p (fairchild670, "mix"), 40.0f },

                { p (eq, "band1_freq"), 110.0f },
                { p (eq, "band7_gain"), 3.5f },
            }
        };
    }

    FactoryPreset warmVocal()
    {
        using namespace ModuleIDs;
        return {
            "Warm Vocal",
            V {
                { p (noiseGate, "threshold"), -48.0f },

                { p (sslChannel, "lowMidGain"), 1.5f },
                { p (sslChannel, "highGain"), -1.0f },
                { p (sslChannel, "threshold"), -12.0f },
                { p (sslChannel, "ratio"), 2.5f },
                { p (sslChannel, "makeup"), 1.0f },

                { p (deEsser1, "threshold"), -20.0f },
                { p (deEsser1, "range"), 8.0f },

                { p (saturation, "drive"), 5.0f },
                { p (saturation, "type"), 0.0f },
                { p (saturation, "tone"), -0.3f },
                { p (saturation, "mix"), 60.0f },

                { p (fet1176, "bypass"), 1.0f },

                { p (fairchild670, "threshold"), -16.0f },
                { p (fairchild670, "ratio"), 3.0f },
                { p (fairchild670, "attack"), 60.0f },
                { p (fairchild670, "release"), 1200.0f },
                { p (fairchild670, "mix"), 70.0f },

                { p (eq, "band2_gain"), 1.5f },
                { p (eq, "band7_gain"), -1.0f },
            }
        };
    }

    FactoryPreset leadVocal()
    {
        using namespace ModuleIDs;
        return {
            "Lead Vocal",
            V {
                { p (noiseGate, "threshold"), -45.0f },

                { p (sslChannel, "lowFreq"), 110.0f },
                { p (sslChannel, "lowGain"), -1.0f },
                { p (sslChannel, "threshold"), -14.0f },
                { p (sslChannel, "ratio"), 3.5f },
                { p (sslChannel, "attack"), 10.0f },
                { p (sslChannel, "release"), 110.0f },
                { p (sslChannel, "makeup"), 2.0f },

                { p (deEsser1, "threshold"), -22.0f },
                { p (deEsser1, "range"), 10.0f },

                { p (saturation, "drive"), 7.0f },
                { p (saturation, "type"), 2.0f },
                { p (saturation, "mix"), 30.0f },

                { p (fet1176, "threshold"), -20.0f },
                { p (fet1176, "ratio"), 1.0f },
                { p (fet1176, "mix"), 60.0f },

                { p (fairchild670, "threshold"), -17.0f },
                { p (fairchild670, "ratio"), 2.5f },
                { p (fairchild670, "mix"), 35.0f },

                { p (eq, "band7_gain"), 2.0f },
                { p (eq, "band8_freq"), 15000.0f },
            }
        };
    }

    FactoryPreset adlib()
    {
        using namespace ModuleIDs;
        return {
            "Adlib",
            V {
                { p (noiseGate, "threshold"), -40.0f },
                { p (noiseGate, "hold"), 30.0f },
                { p (noiseGate, "release"), 100.0f },

                { p (sslChannel, "threshold"), -16.0f },
                { p (sslChannel, "ratio"), 4.0f },
                { p (sslChannel, "attack"), 6.0f },

                { p (deEsser1, "threshold"), -24.0f },
                { p (deEsser1, "range"), 12.0f },
                { p (deEsser1, "attack"), 1.0f },

                { p (saturation, "drive"), 10.0f },
                { p (saturation, "type"), 2.0f },
                { p (saturation, "tone"), 0.2f },
                { p (saturation, "mix"), 45.0f },

                { p (fet1176, "threshold"), -22.0f },
                { p (fet1176, "ratio"), 2.0f },
                { p (fet1176, "attack"), 0.05f },
                { p (fet1176, "release"), 100.0f },
                { p (fet1176, "mix"), 90.0f },

                { p (fairchild670, "bypass"), 1.0f },

                { p (eq, "band1_freq"), 120.0f },
                { p (eq, "band7_gain"), 3.0f },
                { p (eq, "band8_freq"), 16000.0f },
            }
        };
    }

    FactoryPreset darkVocal()
    {
        using namespace ModuleIDs;
        return {
            "Dark Vocal",
            V {
                { p (noiseGate, "threshold"), -48.0f },

                { p (sslChannel, "highGain"), -3.0f },
                { p (sslChannel, "lowMidGain"), 1.5f },
                { p (sslChannel, "highMidGain"), -1.0f },

                { p (deEsser1, "threshold"), -18.0f },
                { p (deEsser1, "range"), 6.0f },

                { p (saturation, "drive"), 6.0f },
                { p (saturation, "type"), 1.0f },
                { p (saturation, "tone"), -0.5f },
                { p (saturation, "mix"), 50.0f },

                { p (fet1176, "bypass"), 1.0f },

                { p (fairchild670, "threshold"), -16.0f },
                { p (fairchild670, "ratio"), 3.0f },
                { p (fairchild670, "attack"), 70.0f },
                { p (fairchild670, "release"), 1500.0f },
                { p (fairchild670, "mix"), 60.0f },

                { p (eq, "band2_gain"), 2.0f },
                { p (eq, "band7_freq"), 7000.0f },
                { p (eq, "band7_gain"), -2.5f },
            }
        };
    }

    FactoryPreset airyVocal()
    {
        using namespace ModuleIDs;
        return {
            "Airy Vocal",
            V {
                { p (noiseGate, "threshold"), -50.0f },

                { p (sslChannel, "hpf"), 100.0f },
                { p (sslChannel, "lowGain"), -1.0f },
                { p (sslChannel, "highGain"), 2.5f },
                { p (sslChannel, "threshold"), -10.0f },
                { p (sslChannel, "ratio"), 2.0f },

                { p (deEsser1, "threshold"), -22.0f },
                { p (deEsser1, "range"), 10.0f },

                { p (saturation, "drive"), 3.0f },
                { p (saturation, "type"), 0.0f },
                { p (saturation, "tone"), 0.3f },
                { p (saturation, "mix"), 25.0f },

                { p (fet1176, "threshold"), -18.0f },
                { p (fet1176, "ratio"), 0.0f },
                { p (fet1176, "mix"), 40.0f },

                { p (fairchild670, "bypass"), 1.0f },

                { p (eq, "band1_freq"), 100.0f },
                { p (eq, "band7_gain"), 3.5f },
                { p (eq, "band8_freq"), 16500.0f },
            }
        };
    }
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Default", {} },   // toutes les valeurs par defaut, sans exception
        cleanVocal(),
        rapVocal(),
        aggressiveVocal(),
        warmVocal(),
        leadVocal(),
        adlib(),
        darkVocal(),
        airyVocal(),
    };

    return presets;
}

} // namespace onirique
