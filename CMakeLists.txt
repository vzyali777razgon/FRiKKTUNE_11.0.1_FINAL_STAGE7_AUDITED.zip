cmake_minimum_required(VERSION 3.22)
project(FRiKKTUNE VERSION 11.0.1 LANGUAGES CXX)

option(FRIKKTUNE_BUILD_DETECTOR_TESTS "Build standalone pitch detector tests" ON)
option(FRIKKTUNE_BUILD_PLUGIN "Build the JUCE VST3 plugin (requires JUCE download/source)" ON)
option(FRIKKTUNE_BUILD_SHIFTER_TESTS "Build standalone phase-vocoder audio tests (test FFT shim only)" ON)

if(FRIKKTUNE_BUILD_DETECTOR_TESTS)
    enable_testing()
    add_executable(FRiKKTUNE_PitchDetectorTests Tests/PitchDetectorTests.cpp)
    target_compile_features(FRiKKTUNE_PitchDetectorTests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(FRiKKTUNE_PitchDetectorTests PRIVATE /W4)
    else()
        target_compile_options(FRiKKTUNE_PitchDetectorTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME FRiKKTUNE_PitchDetectorTests COMMAND FRiKKTUNE_PitchDetectorTests)
endif()

if(FRIKKTUNE_BUILD_SHIFTER_TESTS)
    enable_testing()
    add_executable(FRiKKTUNE_PitchShifterAudioTests Tests/PitchShifterAudioTests.cpp)
    target_include_directories(FRiKKTUNE_PitchShifterAudioTests BEFORE PRIVATE Tests/juce_stub)
    target_compile_features(FRiKKTUNE_PitchShifterAudioTests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(FRiKKTUNE_PitchShifterAudioTests PRIVATE /W4)
    else()
        target_compile_options(FRiKKTUNE_PitchShifterAudioTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME FRiKKTUNE_PitchShifterAudioTests COMMAND FRiKKTUNE_PitchShifterAudioTests)
endif()

if(FRIKKTUNE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(FRiKKTUNE
        COMPANY_NAME "FRiKK Audio" PRODUCT_NAME "FRiKKTUNE"
        PLUGIN_MANUFACTURER_CODE FrKK PLUGIN_CODE FrKt
        FORMATS VST3
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES "Fx")
    juce_generate_juce_header(FRiKKTUNE)
    target_sources(FRiKKTUNE PRIVATE
        Source/PluginProcessor.cpp Source/PluginProcessor.h
        Source/PluginEditor.cpp Source/PluginEditor.h
        Source/PitchDetector.h Source/NoteQuantizer.h Source/PitchShifter.h Source/PitchJumpGate.h Source/NoteTargetTracker.h)
    target_compile_features(FRiKKTUNE PRIVATE cxx_std_17)
    target_link_libraries(FRiKKTUNE PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_compile_definitions(FRiKKTUNE PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endif()
