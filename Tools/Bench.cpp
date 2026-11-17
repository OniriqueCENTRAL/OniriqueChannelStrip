// Outil de développement : instancie le plugin sans DAW.
//
//   OniriqueChannelStripBench render <dossier>             une voix de synthèse passe dans chaque module, seul puis
//                                                          tous ensemble, à 32, 512 et 2048 échantillons par bloc ;
//                                                          un .wav par essai dans <dossier>
//   OniriqueChannelStripBench compare <avant> <après>      écart entre deux rendus, essai par essai, et écart entre
//                                                          les tailles de bloc d'un même rendu
//   OniriqueChannelStripBench shot <sortie.png> [échelle]  l'interface, tous les modules allumés, dans une image

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <iostream>

#if JUCE_WINDOWS
 #define NOMINMAX
 #include <windows.h>
#endif

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double sampleRate = 48000.0;
const int blockSizes[] = { 32, 512, 2048 };

void set (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    else
        std::cout << "parametre inconnu : " << id << std::endl;
}

/** 4 s de voix de synthèse : syllabes sur une fondamentale qui glisse, sifflantes, souffle entre les phrases. */
juce::AudioBuffer<float> voice()
{
    const int n = (int) (4.0 * sampleRate);
    juce::AudioBuffer<float> b (2, n);
    juce::Random rnd (7);
    double phase = 0.0;
    float hp = 0.0f, prev = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sampleRate;
        const double f0 = 170.0 * std::pow (2.0, (std::floor (t * 2.0) * 3.0 - 6.0 * std::floor (t * 0.5)) / 12.0)
                        * (1.0 + 0.012 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.5 * t));
        phase += f0 / sampleRate;
        double v = 0.0;
        for (int h = 1; h <= 30; ++h)
            if (h * f0 < 16000.0)
                v += std::sin (2.0 * juce::MathConstants<double>::pi * h * phase) / std::pow ((double) h, 1.1) * (h >= 3 && h <= 5 ? 1.6 : 1.0);
        const double syl = std::fmod (t, 0.5);
        const double env = (t > 3.4 ? 0.0 : 1.0) * std::pow (std::sin (juce::MathConstants<double>::pi * juce::jlimit (0.0, 1.0, syl / 0.38)), 0.6);
        // sifflante : bruit passe-haut à la fin de chaque syllabe
        const float w = rnd.nextFloat() * 2.0f - 1.0f;
        hp = 0.82f * (hp + w - prev); prev = w;
        const double ess = (syl > 0.36 && syl < 0.46) ? 0.55 : 0.0;
        const float s = (float) (0.16 * env * v + ess * hp * 0.5 + 0.0015 * w);
        b.setSample (0, i, s);
        b.setSample (1, i, s * 0.97f);
    }
    return b;
}

struct Trial { const char* name; std::function<void (juce::AudioProcessorValueTreeState&)> setup; };

std::vector<Trial> trials()
{
    using namespace onirique::ModuleIDs;
    auto on = [] (juce::AudioProcessorValueTreeState& s, const juce::String& m) { set (s, m + "_bypass", 0.0f); };
    return {
        { "gate",       [=] (auto& s) { on (s, noiseGate); set (s, noiseGate + "_threshold", -40.0f); } },
        { "ssl",        [=] (auto& s) { on (s, sslChannel); set (s, sslChannel + "_threshold", -22.0f); set (s, sslChannel + "_highGain", 4.0f); } },
        { "deesser",    [=] (auto& s) { on (s, deEsser1); set (s, deEsser1 + "_threshold", -34.0f); } },
        { "saturation", [=] (auto& s) { on (s, saturation); set (s, saturation + "_drive", 14.0f); } },
        { "fet",        [=] (auto& s) { on (s, fet1176); set (s, fet1176 + "_threshold", -28.0f); set (s, fet1176 + "_autoGain", 1.0f); } },
        { "fairchild",  [=] (auto& s) { on (s, fairchild670); set (s, fairchild670 + "_threshold", -30.0f); set (s, fairchild670 + "_ratio", 4.0f); } },
        { "eq",         [=] (auto& s) { on (s, eq); } },
        { "tout",       [=] (auto& s) { for (auto& t : trials()) if (juce::String (t.name) != "tout") t.setup (s); } },
    };
}

juce::AudioBuffer<float> render (const Trial& trial, int blockSize, const juce::AudioBuffer<float>& in)
{
    std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
    auto& state = dynamic_cast<OniriqueChannelStripProcessor&> (*plugin).apvts;
    trial.setup (state);
    plugin->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += blockSize)
    {
        const int len = juce::jmin (blockSize, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, len);
        plugin->processBlock (view, midi);
    }
    // aligne sur l'entrée : retire la latence déclarée
    const int lat = plugin->getLatencySamples();
    if (lat > 0)
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = out.getWritePointer (ch);
            std::memmove (d, d + lat, sizeof (float) * (size_t) (out.getNumSamples() - lat));
            juce::FloatVectorOperations::clear (d + out.getNumSamples() - lat, lat);
        }
    return out;
}

bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sampleRate, 2, 32, {}, 0));
    return w != nullptr && w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

juce::AudioBuffer<float> readWav (const juce::File& f)
{
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (new juce::FileInputStream (f), true));
    juce::AudioBuffer<float> b;
    if (r == nullptr) return b;
    b.setSize (2, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    return b;
}

/** Laisse tourner les messages de l'interface (minuteries, animations) pendant ms millisecondes. */
void pump (int ms)
{
   #if JUCE_WINDOWS
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
    while (juce::Time::getMillisecondCounter() < end)
    {
        MSG msg;
        while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage (&msg); DispatchMessageW (&msg); }
        juce::Thread::sleep (1);
    }
   #else
    juce::Thread::sleep (ms);
   #endif
}

double rmsDb (const juce::AudioBuffer<float>& b)
{
    double s = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            s += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return 10.0 * std::log10 (s / juce::jmax (1, b.getNumChannels() * b.getNumSamples()) + 1.0e-20);
}

/** Écart entre deux rendus, en dB sous le niveau du premier. */
double diffDb (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    juce::AudioBuffer<float> d (a);
    for (int ch = 0; ch < 2; ++ch)
        d.addFrom (ch, 0, b, ch, 0, juce::jmin (a.getNumSamples(), b.getNumSamples()), -1.0f);
    return rmsDb (d) - rmsDb (a);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String mode = argc > 1 ? argv[1] : "";
    const auto cwd = juce::File::getCurrentWorkingDirectory();

    if (mode == "render" && argc == 3)
    {
        const auto dir = cwd.getChildFile (argv[2]);
        dir.createDirectory();
        const auto in = voice();
        writeWav (dir.getChildFile ("entree.wav"), in);
        for (auto& t : trials())
            for (int bs : blockSizes)
            {
                const auto out = render (t, bs, in);
                writeWav (dir.getChildFile (juce::String (t.name) + "_" + juce::String (bs) + ".wav"), out);
                std::cout << t.name << " bloc " << bs << " : " << juce::String (rmsDb (out), 2) << " dB RMS" << std::endl;
            }
        return 0;
    }

    if (mode == "compare" && argc == 4)
    {
        const auto a = cwd.getChildFile (argv[2]), b = cwd.getChildFile (argv[3]);
        std::cout << "essai        avant/apres (bloc 512)   bloc 32 vs 2048 : avant   apres" << std::endl;
        for (auto& t : trials())
        {
            const auto name = juce::String (t.name);
            const auto ab = diffDb (readWav (a.getChildFile (name + "_512.wav")), readWav (b.getChildFile (name + "_512.wav")));
            const auto ba = diffDb (readWav (a.getChildFile (name + "_32.wav")), readWav (a.getChildFile (name + "_2048.wav")));
            const auto bb = diffDb (readWav (b.getChildFile (name + "_32.wav")), readWav (b.getChildFile (name + "_2048.wav")));
            std::cout << name.paddedRight (' ', 12) << " " << juce::String (ab, 1).paddedLeft (' ', 8) << " dB"
                      << juce::String (ba, 1).paddedLeft (' ', 28) << " dB" << juce::String (bb, 1).paddedLeft (' ', 9) << " dB" << std::endl;
        }
        return 0;
    }

    if (mode == "shot" && argc >= 3)
    {
        const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;
        std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
        auto& state = dynamic_cast<OniriqueChannelStripProcessor&> (*plugin).apvts;
        for (auto& t : trials())
            if (juce::String (t.name) == "tout")
                t.setup (state);
        const int bs = 512;
        plugin->setPlayConfigDetails (2, 2, sampleRate, bs);
        plugin->prepareToPlay (sampleRate, bs);
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditor());

        // la voix tourne pendant que l'interface vit (meters, animations d'ouverture)
        auto in = voice();
        juce::MidiBuffer midi;
        for (int pos = 0; pos + bs <= in.getNumSamples() * 3 / 4; pos += bs)
        {
            juce::AudioBuffer<float> view (in.getArrayOfWritePointers(), 2, pos, bs);
            plugin->processBlock (view, midi);
            pump (bs * 1000 / (int) sampleRate);
        }
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        const auto file = cwd.getChildFile (argv[2]);
        file.deleteFile();
        juce::FileOutputStream out (file);
        const bool ok = juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << file.getFullPathName() << " " << image.getWidth() << "x" << image.getHeight() << std::endl;
        return ok ? 0 : 1;
    }

    std::cout << "usage : OniriqueChannelStripBench render <dossier> | compare <avant> <apres> | shot <sortie.png> [echelle]" << std::endl;
    return 2;
}
