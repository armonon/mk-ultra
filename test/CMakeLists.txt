# Build the engine as a small static lib so both the plugin and tests can share it.
# (Here we simply compile the engine source directly into the test target, since it
#  only depends on juce_audio_basics and juce_dsp.)

add_executable(GrainFreezeTests
    source/GranularEngineTests.cpp
    source/MidiNoteControllerTests.cpp
    source/MixEngineTests.cpp
    source/PrettifierEngineTests.cpp
    source/AntiAliasTests.cpp
    source/UpdateCheckerTests.cpp
    ${CMAKE_SOURCE_DIR}/plugin/source/GranularEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugin/source/Entropy/EntropyEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugin/source/Mix/MixEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugin/source/Prettifier/PrettifierEngine.cpp
    ${CMAKE_SOURCE_DIR}/plugin/source/ModMatrix.cpp
    ${CMAKE_SOURCE_DIR}/plugin/source/SpectralFreeze.cpp
)

target_include_directories(GrainFreezeTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin/include
)

target_link_libraries(GrainFreezeTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# JUCE modules need these defined when compiled outside a juce_add_* target.
target_compile_definitions(GrainFreezeTests
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

include(CTest)
add_test(NAME GrainFreezeTests COMMAND GrainFreezeTests)
