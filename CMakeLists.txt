cmake_minimum_required(VERSION 3.22)

# ---- product identity: change these to rename the plugin --------------------------------------------
set(PS_PRODUCT_NAME  "PitchSnap")
set(PS_COMPANY_NAME  "DIY")
set(PS_MANUFACTURER_CODE "Diyz")   # 4 chars, at least one UPPER-case
set(PS_PLUGIN_CODE       "Psnp")   # 4 chars, exactly one UPPER-case, must be unique
# -----------------------------------------------------------------------------------------------------------

# Static MSVC runtime: the plugin then needs no VC++ redistributable on the user's PC.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(PitchSnap VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if -DJUCE_DIR=... is given, otherwise download it.
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                              GIT_TAG 9.0.2 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PitchSnap
    COMPANY_NAME            "${PS_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE ${PS_MANUFACTURER_CODE}
    PLUGIN_CODE              ${PS_PLUGIN_CODE}
    FORMATS                  VST3
    PRODUCT_NAME             "${PS_PRODUCT_NAME}"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(PitchSnap PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PitchSnap PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PitchSnap
    PRIVATE juce::juce_audio_utils
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

# MinGW cross build: no runtime DLLs next to the plugin (FL Studio would fail to load it).
if(MINGW)
    target_link_options(PitchSnap PRIVATE -static -static-libgcc -static-libstdc++)
    target_compile_options(PitchSnap PRIVATE -Wa,-mbig-obj)
endif()

# ---- optional: headless integration test of the processor (no GUI needed) ---------------------------------
option(PS_BUILD_TESTS "Build the headless wrapper test" OFF)
if(PS_BUILD_TESTS)
    juce_add_console_app(PitchSnapWrapperTest PRODUCT_NAME "wrapper_test")
    target_sources(PitchSnapWrapperTest PRIVATE tests/wrapper_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(PitchSnapWrapperTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="PitchSnap")
    target_link_libraries(PitchSnapWrapperTest PRIVATE juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
