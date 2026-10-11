cmake_minimum_required(VERSION 3.22)

# Mac : binaire universel (Intel + Apple Silicon), a definir avant project()
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(OniriqueChannelStrip VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
#  JUCE 8 (telecharge automatiquement depuis GitHub : necessite Git + Internet)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# AU (Logic, GarageBand) uniquement sur Mac
set(ONIRIQUE_FORMATS VST3)
if(APPLE)
    list(APPEND ONIRIQUE_FORMATS AU)
endif()

# ---------------------------------------------------------------------------
#  Sigil ONIRIQUE (meme asset que le VocalBox) : integre en BinaryData plutot que charge
#  depuis un fichier au runtime (plus simple a distribuer avec le .vst3).
# ---------------------------------------------------------------------------
juce_add_binary_data(OniriqueChannelStripData SOURCES Source/GUI/Assets/sigil.png)
set_target_properties(OniriqueChannelStripData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

juce_add_plugin(OniriqueChannelStrip
    COMPANY_NAME               "Onirique"
    PLUGIN_MANUFACTURER_CODE   Onrq
    PLUGIN_CODE                Ocs1
    FORMATS                    ${ONIRIQUE_FORMATS}
    AU_MAIN_TYPE               kAudioUnitType_Effect
    PRODUCT_NAME                "ONIRIQUE CHANNEL STRIP"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES            Fx Dynamics EQ
    COPY_PLUGIN_AFTER_BUILD    FALSE)

target_sources(OniriqueChannelStrip PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.cpp
    Source/Parameters.h
    Source/Presets.h
    Source/Presets.cpp
    Source/DSP/ProcessorModule.h
    Source/DSP/DSPHelpers.h
    Source/DSP/ModuleChain.h
    Source/DSP/ModuleChain.cpp
    Source/DSP/NoiseGateModule.h
    Source/DSP/NoiseGateModule.cpp
    Source/DSP/SSLChannelModule.h
    Source/DSP/SSLChannelModule.cpp
    Source/DSP/DeEsserModule.h
    Source/DSP/DeEsserModule.cpp
    Source/DSP/SaturationModule.h
    Source/DSP/SaturationModule.cpp
    Source/DSP/FETCompressorModule.h
    Source/DSP/FETCompressorModule.cpp
    Source/DSP/VariMuCompressorModule.h
    Source/DSP/VariMuCompressorModule.cpp
    Source/DSP/ParametricEQModule.h
    Source/DSP/ParametricEQModule.cpp
    Source/DSP/SpectrumAnalyzer.h
    Source/DSP/SpectrumAnalyzer.cpp
    Source/GUI/UIStyle.h
    Source/GUI/OniriqueLookAndFeel.h
    Source/GUI/OniriqueLookAndFeel.cpp
    Source/GUI/ModuleOrderManager.h
    Source/GUI/ModuleOrderManager.cpp
    Source/GUI/OrderBadge.h
    Source/GUI/OrderBadge.cpp
    Source/GUI/ParamControls.h
    Source/GUI/ParamControls.cpp
    Source/GUI/ModulePanel.h
    Source/GUI/ModulePanel.cpp
    Source/GUI/MeterBar.h
    Source/GUI/MeterBar.cpp
    Source/GUI/TransferCurveDisplay.h
    Source/GUI/TransferCurveDisplay.cpp
    Source/GUI/DualDeEsserPanel.h
    Source/GUI/DualDeEsserPanel.cpp)

target_include_directories(OniriqueChannelStrip PRIVATE Source)

target_compile_definitions(OniriqueChannelStrip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

if(MSVC)
    target_compile_options(OniriqueChannelStrip PRIVATE /utf-8)
endif()

target_link_libraries(OniriqueChannelStrip
    PRIVATE
        OniriqueChannelStripData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
