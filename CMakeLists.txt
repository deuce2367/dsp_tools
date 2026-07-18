cmake_minimum_required(VERSION 3.14)
project(dsp_tools VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Suppress warnings from third-party libraries (KFR, spdlog, fmt)
add_compile_options(-Wno-attributes)

option(DSP_TOOLS_COVERAGE "Enable code coverage reporting" OFF)

# Suppress deprecation warnings from third-party FetchContent modules globally
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

if(POLICY CMP0148)
    cmake_policy(SET CMP0148 OLD)
    set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
endif()

include(FetchContent)

# KFR
set(KFR_ENABLE_DFT ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    kfr
    GIT_REPOSITORY https://github.com/kfrlib/kfr.git
    GIT_TAG        6.0.2
)
FetchContent_MakeAvailable(kfr)

# Fetch CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.3.2
)
FetchContent_MakeAvailable(cli11)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# stb
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
  FetchContent_Populate(stb)
endif()

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# pybind11
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Catch2
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(dsp_plotter
    main.cpp
    dsp_engine.cpp
    plot_generator.cpp
)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_plotter PUBLIC OpenMP::OpenMP_CXX)
endif()

target_link_libraries(dsp_plotter
  PRIVATE
    kfr
    kfr_dft
    kfr_io
    kfr_dsp
    CLI11::CLI11
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_compile_options(dsp_plotter PRIVATE -Wno-stringop-overflow)
target_include_directories(dsp_plotter PRIVATE ${stb_SOURCE_DIR})

add_executable(dsp_resample
    dsp_resample.cpp
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_resample PUBLIC OpenMP::OpenMP_CXX)
endif()

target_link_libraries(dsp_resample
  PRIVATE
    kfr
    kfr_dsp
    kfr_io
    CLI11::CLI11
    spdlog::spdlog
)
target_compile_options(dsp_resample PRIVATE -Wno-stringop-overflow)

add_executable(dsp_filter dsp_filter.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_filter PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_filter PRIVATE kfr kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_filter PRIVATE -Wno-stringop-overflow)

add_executable(dsp_tuner dsp_tuner.cpp dsp_tuner_lib.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_tuner PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_tuner PRIVATE kfr kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_tuner PRIVATE -Wno-stringop-overflow)

add_executable(dsp_format dsp_format.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_format PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_format PRIVATE kfr kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_format PRIVATE -Wno-stringop-overflow)

add_executable(dsp_convert dsp_convert.cpp dsp_convert_lib.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_convert PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_convert PRIVATE kfr kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_convert PRIVATE -Wno-stringop-overflow)

add_executable(dsp_whitener dsp_whitener.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_whitener PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_whitener PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_whitener PRIVATE -Wno-stringop-overflow)

add_executable(dsp_fft dsp_fft.cpp dsp_engine.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_fft PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_fft PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_fft PRIVATE -Wno-stringop-overflow)

add_executable(dsp_psd dsp_psd.cpp dsp_engine.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_psd PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_psd PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_psd PRIVATE -Wno-stringop-overflow)

# Python bindings
pybind11_add_module(dsp_plotter_py python_bindings.cpp dsp_engine.cpp plot_generator.cpp dsp_convert_lib.cpp dsp_tuner_lib.cpp)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(dsp_plotter_py PRIVATE "-flto=auto")
    target_link_options(dsp_plotter_py PRIVATE "-flto=auto")
endif()
target_link_libraries(dsp_plotter_py PRIVATE kfr kfr_dft kfr_io kfr_dsp spdlog::spdlog)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_plotter_py PRIVATE OpenMP::OpenMP_CXX)
endif()
target_compile_options(dsp_plotter_py PRIVATE -Wno-stringop-overflow)
target_include_directories(dsp_plotter_py PRIVATE ${stb_SOURCE_DIR})

# ----------------- TESTS -----------------
enable_testing()
add_executable(dsp_tests
    tests/test_main.cpp
    tests/test_whitener.cpp
    tests/test_filter.cpp
    tests/test_tuner.cpp
    tests/test_resample.cpp
    tests/test_format.cpp
)
target_compile_definitions(dsp_tests PRIVATE DSP_TOOLS_TEST_MODE=1)
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_tests PUBLIC OpenMP::OpenMP_CXX)
endif()
target_link_libraries(dsp_tests PRIVATE Catch2::Catch2WithMain kfr kfr_dft kfr_dsp kfr_io spdlog::spdlog CLI11::CLI11)
target_compile_options(dsp_tests PRIVATE -Wno-stringop-overflow)
catch_discover_tests(dsp_tests)

if(DSP_TOOLS_COVERAGE)
    target_compile_options(dsp_tests PRIVATE --coverage -O0 -g)
    target_link_options(dsp_tests PRIVATE --coverage)
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch --ignore-errors unused
        COMMAND lcov --remove coverage.info '/usr/*' '*/_deps/*' '*/tests/*' --output-file coverage.info
        COMMAND genhtml coverage.info --output-directory coverage
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report..."
    )
endif()
