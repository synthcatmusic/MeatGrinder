cmake_minimum_required(VERSION 3.22)

# Universal binary on macOS (must be set before project())
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

project(MeatGrinder VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

option(MEATGRINDER_COPY_PLUGIN "Copy the VST3 into the system VST3 folder after building" OFF)
option(MEATGRINDER_BUILD_TESTS "Build the DSP test suite and the UI snapshot tool" ON)
set(MEATGRINDER_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout (otherwise JUCE is downloaded)")

# ---------------------------------------------------------------------------- JUCE
if(MEATGRINDER_JUCE_PATH)
    add_subdirectory(${MEATGRINDER_JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.8
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------- assets (fonts, SIL OFL)
juce_add_binary_data(MeatGrinderAssets SOURCES
    Assets/Fonts/BarlowCondensed-SemiBold.ttf
    Assets/Fonts/BarlowCondensed-ExtraBold.ttf)
set_target_properties(MeatGrinderAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------- plugin
juce_add_plugin(MeatGrinder
    COMPANY_NAME                "Cleaver Audio"
    PRODUCT_NAME                "MEATGRINDER"
    DESCRIPTION                 "Analog saturation and harmonic enhancer"
    BUNDLE_ID                   com.cleaveraudio.meatgrinder
    PLUGIN_MANUFACTURER_CODE    Clvr
    PLUGIN_CODE                 Mgrn
    FORMATS                     VST3
    VST3_CATEGORIES             Fx Distortion
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${MEATGRINDER_COPY_PLUGIN})

target_sources(MeatGrinder PRIVATE
    Source/MeatGrinderEngine.cpp
    Source/MeatLookAndFeel.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

target_compile_definitions(MeatGrinder PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MeatGrinder
    PRIVATE
        MeatGrinderAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------- tests / tools
if(MEATGRINDER_BUILD_TESTS)
    # Offline DSP verification (engine only - no GUI code)
    add_executable(MeatGrinderDspTests Tests/DspTests.cpp Source/MeatGrinderEngine.cpp)
    target_compile_definitions(MeatGrinderDspTests PRIVATE
        JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(MeatGrinderDspTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)

    # Renders the real editor to a PNG (used for visual checks / CI artefacts)
    add_executable(MeatGrinderUiShot Tests/UiShot.cpp)
    target_link_libraries(MeatGrinderUiShot PRIVATE
        MeatGrinder MeatGrinderAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
