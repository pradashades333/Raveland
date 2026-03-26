cmake_minimum_required(VERSION 3.15)

project(Raveland VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS defaults:
# - Universal binary so hosts running Intel/Rosetta/Apple Silicon can load the plugin.
# - Conservative deployment target to avoid "too new" binaries on client machines.
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS" FORCE)
    endif()

    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

add_subdirectory(extern/JUCE)

juce_add_plugin(Raveland
    COMPANY_NAME "NS Audio"
    COMPANY_WEBSITE "https://nsaudio.example"
    BUNDLE_ID com.nsaudio.raveland
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE NSAu
    PLUGIN_CODE RvLd
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "RaveLand")

target_sources(Raveland
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginProcessor.h
        source/PluginEditor.cpp
        source/PluginEditor.h
        source/SynthVoice.cpp
        source/SynthVoice.h
        source/SampleLayer.h
        source/WaveformDisplay.h
        source/FancyKnob.h)

target_compile_definitions(Raveland
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

if (MSVC)
    target_compile_options(Raveland PRIVATE /EHsc)
else()
    target_compile_options(Raveland PRIVATE -fexceptions)
endif()

target_link_libraries(Raveland
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_core)
