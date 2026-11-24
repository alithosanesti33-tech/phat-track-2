cmake_minimum_required(VERSION 3.22)
project(PhatTrack VERSION 0.3.0)

set(JUCE_PATH "" CACHE PATH "Path to the JUCE source folder")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_plugin(PhatTrack
    COMPANY_NAME "Open Circuit Audio"
    BUNDLE_ID com.opencircuitaudio.phattrack
    PLUGIN_MANUFACTURER_CODE OpCa
    PLUGIN_CODE PhTk
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "PhatTrack"
    DESCRIPTION "Guitar-to-synth tracking monosynth"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(PhatTrack)

target_sources(PhatTrack PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_compile_features(PhatTrack PRIVATE cxx_std_17)
target_compile_definitions(PhatTrack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(PhatTrack PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags
)
