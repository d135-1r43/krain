# Header-only WAV writer, shared by the tests and the render tool.
add_library(graindelay_wav INTERFACE)
target_include_directories(graindelay_wav INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Where renders land. The compare app is served straight out of this directory.
set(GRAINDELAY_RENDER_DIR "${CMAKE_BINARY_DIR}/renders" CACHE PATH "Directory for rendered WAV comparisons")
file(MAKE_DIRECTORY ${GRAINDELAY_RENDER_DIR})

add_executable(graindelay-render render/RenderMain.cpp)

target_link_libraries(graindelay-render
    PRIVATE
        GrainDelayDsp
        graindelay_wav
        graindelay_warnings
        juce::juce_audio_formats
        juce::juce_recommended_config_flags)

target_compile_definitions(graindelay-render PRIVATE
    GRAINDELAY_RENDER_DIR="${GRAINDELAY_RENDER_DIR}"
    JUCE_STANDALONE_APPLICATION=1)

# Keep the compare app next to the renders it loads, so the whole directory can be
# served by any static file server.
add_custom_command(TARGET graindelay-render POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/compare/index.html
            ${GRAINDELAY_RENDER_DIR}/index.html
    COMMENT "Copying the compare app next to the renders")
