cmake_minimum_required(VERSION 3.22)
project(DISRUPTION VERSION 1.0.0 LANGUAGES C CXX)
set(DISRUPTION_RC_LABEL "" CACHE STRING "Optional internal release-candidate designation")
set(DISRUPTION_DISPLAY_VERSION "${PROJECT_VERSION}")
if(DISRUPTION_RC_LABEL)
 string(APPEND DISRUPTION_DISPLAY_VERSION " ${DISRUPTION_RC_LABEL}")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(DISRUPTION_JUCE_PATH "" CACHE PATH "Existing JUCE 8 checkout")
if(DISRUPTION_JUCE_PATH)
 add_subdirectory("${DISRUPTION_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
 FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(DISRUPTION COMPANY_NAME VoxxLab BUNDLE_ID com.voxxlab.disruption
 PLUGIN_MANUFACTURER_CODE VxLb PLUGIN_CODE Dsrp PRODUCT_NAME DISRUPTION
 DESCRIPTION "Multi-mode audio destruction effect" FORMATS VST3 IS_SYNTH FALSE
 NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Fx Distortion)
if(WIN32)
 # JUCE 8.0.12's rcfile custom command omits its Info.txt dependency.
 # Regenerate Windows version resources when project metadata changes.
 get_target_property(disruption_generated_dir DISRUPTION JUCE_GENERATED_SOURCES_DIRECTORY)
 get_target_property(disruption_info_file DISRUPTION JUCE_INFO_FILE)
 add_custom_command(OUTPUT "${disruption_generated_dir}/DISRUPTION_resources.rc"
  APPEND DEPENDS "${disruption_info_file}")
endif()
juce_add_binary_data(DisruptionPresets SOURCES Resources/FactoryPresets.xml Resources/DisruptionArtwork.png)
file(GLOB UI_SOURCES CONFIGURE_DEPENDS Source/UI/*.cpp)
file(GLOB DSP_SOURCES CONFIGURE_DEPENDS Source/DSP/*.cpp)
target_sources(DISRUPTION PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets/PresetManager.cpp ${DSP_SOURCES} ${UI_SOURCES})
target_include_directories(DISRUPTION PUBLIC Source)
target_compile_features(DISRUPTION PUBLIC cxx_std_17)
target_compile_definitions(DISRUPTION PUBLIC DISRUPTION_DISPLAY_VERSION="${DISRUPTION_DISPLAY_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(DISRUPTION PRIVATE juce::juce_audio_utils PUBLIC DisruptionPresets juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
option(DISRUPTION_BUILD_TESTS "Build DSP and actual VST3 host tests" ON)
if(DISRUPTION_BUILD_TESTS)
 enable_testing()
 add_executable(DisruptionTests Tests/DisruptionTests.cpp Tests/Phase2Tests.cpp Tests/Phase3Tests.cpp Tests/Phase4Tests.cpp Tests/Phase5Tests.cpp Tests/Phase6Tests.cpp Tests/Phase7Tests.cpp Tests/Phase8Tests.cpp)
 target_link_libraries(DisruptionTests PRIVATE DISRUPTION juce::juce_audio_utils)
 target_compile_definitions(DisruptionTests PRIVATE JUCE_PLUGINHOST_VST3=1)
 add_dependencies(DisruptionTests DISRUPTION_VST3)
 add_test(NAME DisruptionTests COMMAND DisruptionTests "${CMAKE_BINARY_DIR}/DISRUPTION_artefacts/$<CONFIG>/VST3/DISRUPTION.vst3")
endif()




