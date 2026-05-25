cmake_minimum_required(VERSION 3.14)
project(dsp_plotter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress deprecation warnings from third-party FetchContent modules globally
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

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
