include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.9.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(KrainTests
    GrainEngineTests.cpp)

target_link_libraries(KrainTests
    PRIVATE
        KrainDsp
        krain_wav
        krain_warnings
        juce::juce_recommended_config_flags
        Catch2::Catch2WithMain)

# Tests drop listenable source/processed pairs next to the renderer's output, so
# `ctest` and `krain-render` both feed the same compare app.
target_compile_definitions(KrainTests PRIVATE KRAIN_RENDER_DIR="${KRAIN_RENDER_DIR}")

# catch_discover_tests registers every TEST_CASE with ctest individually.
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(KrainTests)
