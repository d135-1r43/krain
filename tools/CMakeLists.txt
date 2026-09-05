# Header-only WAV writer, shared by the tests and the render tool.
add_library(krain_wav INTERFACE)
target_include_directories(krain_wav INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Where renders land. The compare app is served straight out of this directory.
set(KRAIN_RENDER_DIR "${CMAKE_BINARY_DIR}/renders" CACHE PATH "Directory for rendered WAV comparisons")
file(MAKE_DIRECTORY ${KRAIN_RENDER_DIR})

add_executable(krain-render render/RenderMain.cpp)

target_link_libraries(krain-render
    PRIVATE
        KrainDsp
        krain_wav
        krain_warnings
        juce::juce_audio_formats
        juce::juce_recommended_config_flags)

target_compile_definitions(krain-render PRIVATE
    KRAIN_RENDER_DIR="${KRAIN_RENDER_DIR}"
    JUCE_STANDALONE_APPLICATION=1)

# Keep the compare app next to the renders it loads, so the whole directory can be
# served by any static file server.
add_custom_command(TARGET krain-render POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/compare/index.html
            ${KRAIN_RENDER_DIR}/index.html
    COMMENT "Copying the compare app next to the renders")
