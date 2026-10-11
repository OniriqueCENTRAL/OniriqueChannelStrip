#include "Parameters.h"

#include "DSP/NoiseGateModule.h"
#include "DSP/SSLChannelModule.h"
#include "DSP/DeEsserModule.h"
#include "DSP/SaturationModule.h"
#include "DSP/FETCompressorModule.h"
#include "DSP/VariMuCompressorModule.h"
#include "DSP/ParametricEQModule.h"

namespace onirique
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    NoiseGateModule::addParameters       (layout, ModuleIDs::noiseGate);
    SSLChannelModule::addParameters      (layout, ModuleIDs::sslChannel);
    DeEsserModule::addParameters         (layout, ModuleIDs::deEsser1, "De-Esser 1", 3);
    DeEsserModule::addParameters         (layout, ModuleIDs::deEsser2, "De-Esser 2", 8);
    SaturationModule::addParameters      (layout, ModuleIDs::saturation);
    FETCompressorModule::addParameters   (layout, ModuleIDs::fet1176);
    VariMuCompressorModule::addParameters(layout, ModuleIDs::fairchild670);
    ParametricEQModule::addParameters    (layout, ModuleIDs::eq);

    return layout;
}

} // namespace onirique
