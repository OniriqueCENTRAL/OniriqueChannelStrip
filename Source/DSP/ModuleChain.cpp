#include "ModuleChain.h"
#include "DSPHelpers.h"

namespace onirique
{

namespace
{
    constexpr double bypassRampSeconds = 0.008;   // ~8 ms : assez court pour rester reactif, assez long pour ne pas clicker
    constexpr double orderFadeSeconds  = 0.006;   // ~6 ms : attenue la discontinuite d'un reordonnancement
}

ProcessorModule& ModuleChain::addModule (std::unique_ptr<ProcessorModule> module)
{
    jassert (module != nullptr);
    modules.push_back (std::move (module));
    bypassRamps.emplace_back();
    return *modules.back();
}

ProcessorModule* ModuleChain::getModule (const juce::String& moduleID) const
{
    for (auto& m : modules)
        if (m->getID() == moduleID)
            return m.get();

    return nullptr;
}

void ModuleChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    preparedSampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;

    for (auto& m : modules)
        m->prepare (spec);

    sortedEntries.clear();
    sortedEntries.reserve (modules.size());
    for (size_t i = 0; i < modules.size(); ++i)
        sortedEntries.push_back ({ modules[i].get(), i });

    bypassRamps.resize (modules.size());
    for (size_t i = 0; i < modules.size(); ++i)
    {
        bypassRamps[i].reset (preparedSampleRate, bypassRampSeconds);
        // Seede la rampe a l'etat de bypass ACTUEL : sans ca, un module deja
        // actif au demarrage ferait un fondu d'entree depuis le silence a
        // chaque prepareToPlay() (ouverture du plugin, changement de sample
        // rate/buffer...), ce qui n'est pas souhaitable.
        bypassRamps[i].setCurrentAndTargetValue (modules[i]->isBypassed() ? 0.0f : 1.0f);
    }

    dryScratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

    orderChangeFade.reset (preparedSampleRate, orderFadeSeconds);
    orderChangeFade.setCurrentAndTargetValue (1.0f);

    rebuildProcessingOrderIfNeeded();
    lastOrderSignature.resize (sortedEntries.size());
    for (size_t i = 0; i < sortedEntries.size(); ++i)
        lastOrderSignature[i] = sortedEntries[i].module;
}

void ModuleChain::reset()
{
    for (auto& m : modules)
        m->reset();
}

int ModuleChain::getTotalLatencySamples() const noexcept
{
    int total = 0;
    for (auto& m : modules)
        total += m->getLatencySamples();
    return total;
}

void ModuleChain::rebuildProcessingOrderIfNeeded()
{
    // Tri par insertion, stable, zero allocation : adapte a un tableau aussi
    // petit (8 modules) et garantit -- contrairement a std::stable_sort, dont
    // le besoin eventuel de memoire temporaire depend de l'implementation de
    // la bibliotheque standard -- qu'aucune allocation ne peut se produire
    // ici, y compris dans le pire cas.
    for (size_t i = 1; i < sortedEntries.size(); ++i)
    {
        const Entry key = sortedEntries[i];
        const int keyOrder = key.module->getOrder();

        size_t j = i;
        while (j > 0 && sortedEntries[j - 1].module->getOrder() > keyOrder)
        {
            sortedEntries[j] = sortedEntries[j - 1];
            --j;
        }
        sortedEntries[j] = key;
    }
}

void ModuleChain::process (juce::dsp::AudioBlock<float>& block)
{
    rebuildProcessingOrderIfNeeded();

    // --- Un reordonnancement reel (pas un simple retri qui retombe sur la
    //     meme sequence) change le chemin du signal d'un bloc a l'autre :
    //     on ne peut pas le masquer par un simple crossfade dry/wet comme
    //     pour le bypass, donc on attenue la discontinuite par un court
    //     fondu de sortie plutot que de laisser passer un saut brut.
    bool orderChanged = lastOrderSignature.size() != sortedEntries.size();
    if (! orderChanged)
        for (size_t i = 0; i < sortedEntries.size(); ++i)
            if (lastOrderSignature[i] != sortedEntries[i].module)
                { orderChanged = true; break; }

    if (orderChanged)
    {
        lastOrderSignature.resize (sortedEntries.size());
        for (size_t i = 0; i < sortedEntries.size(); ++i)
            lastOrderSignature[i] = sortedEntries[i].module;

        orderChangeFade.setCurrentAndTargetValue (0.0f);
        orderChangeFade.setTargetValue (1.0f);
    }

    const auto numCh  = block.getNumChannels();
    const auto numSmp = block.getNumSamples();

    for (auto& entry : sortedEntries)
    {
        auto* m = entry.module;
        auto& ramp = bypassRamps[entry.rampIndex];

        ramp.setTargetValue (m->isBypassed() ? 0.0f : 1.0f);

        const bool settledOff = ! ramp.isSmoothing() && ramp.getCurrentValue() <= 0.0001f;
        const bool settledOn  = ! ramp.isSmoothing() && ramp.getCurrentValue() >= 0.9999f;

        if (settledOff)
        {
            m->resetMeters();   // module inactif : son meter retombe a zero cote interface
            continue;   // bypasse et stable : le module n'est pas du tout appele (cout minimal)
        }

        if (settledOn)
        {
            m->process (block);
            sanitizeBlock (block);
            continue;   // actif et stable : traitement normal, pas de copie supplementaire
        }

        // Transition en cours : traite le signal normalement (il devient le
        // signal "humide"), puis fait un fondu enchaine echantillon par
        // echantillon avec le signal sec conserve a part -- la seule
        // situation ou ce module coute une copie de bloc en plus, et
        // seulement pendant les ~8 ms de la transition.
        dryScratch.setSize ((int) numCh, (int) numSmp, false, false, true);
        for (size_t ch = 0; ch < numCh; ++ch)
            dryScratch.copyFrom ((int) ch, 0, block.getChannelPointer (ch), (int) numSmp);

        m->process (block);
        sanitizeBlock (block);

        for (size_t s = 0; s < numSmp; ++s)
        {
            const float g = ramp.getNextValue();
            for (size_t ch = 0; ch < numCh; ++ch)
            {
                auto* wet = block.getChannelPointer (ch);
                const float dry = dryScratch.getReadPointer ((int) ch)[s];
                wet[s] = dry + g * (wet[s] - dry);
            }
        }
    }

    if (orderChangeFade.isSmoothing())
    {
        for (size_t s = 0; s < numSmp; ++s)
        {
            const float g = orderChangeFade.getNextValue();
            for (size_t ch = 0; ch < numCh; ++ch)
                block.getChannelPointer (ch)[s] *= g;
        }
    }
}

} // namespace onirique
