juce_add_plugin(Entropy
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE Ycmp
    PLUGIN_CODE MkUl
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "MK Ultra"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Compile the logo + background PNGs into the binary so they ship with the
# plugin (no external file paths). Creates a BinaryData namespace/header.
juce_add_binary_data(EntropyAssets
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/logo.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/background.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/prettifier.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/prettifier_logo.png
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets/mix_background.png
)

target_sources(Entropy
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginEditor.cpp
        source/GranularEngine.cpp
        source/Entropy/EntropyEngine.cpp
        source/Mix/MixEngine.cpp
        source/Prettifier/PrettifierEngine.cpp
        source/ModMatrix.cpp
        source/ModPanel.cpp
        source/PresetManager.cpp
        source/Presets/SnapshotManager.cpp
        source/BiohazardLookAndFeel.cpp
        source/SpectralFreeze.cpp
)

target_include_directories(Entropy
    PRIVATE
        include
)

target_compile_definitions(Entropy
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Entropy
    PRIVATE
        EntropyAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl   # macOS 26 (Tahoe) CoreGraphics glyph-flip workaround; remove once Apple/JUCE fix lands
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
