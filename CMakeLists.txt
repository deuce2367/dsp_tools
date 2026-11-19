cmake_minimum_required(VERSION 3.14)
project(dsp_tools VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Suppress warnings from third-party libraries (KFR, spdlog, fmt)
add_compile_options(-Wno-attributes)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-template-body>)
endif()

option(DSP_TOOLS_COVERAGE "Enable code coverage reporting" OFF)
option(DSP_TOOLS_STATIC "Build statically linked CLI executables and statically link third-party libraries" ON)

# Suppress deprecation warnings from third-party FetchContent modules globally
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

include(ProcessorCount)
ProcessorCount(N_CPUS)
if(N_CPUS GREATER 1)
    math(EXPR HALF_CPUS "${N_CPUS} / 2")
else()
    set(HALF_CPUS 1)
endif()

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
    GIT_SHALLOW 1
    GIT_TAG        6.0.2
)
FetchContent_MakeAvailable(kfr)

# Fetch CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_SHALLOW 1
    GIT_TAG        v2.3.2
)
FetchContent_MakeAvailable(cli11)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_SHALLOW 1
    GIT_TAG        v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# stb
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_SHALLOW 1
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_SHALLOW 1
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# pybind11
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_SHALLOW 1
    GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# Catch2
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_SHALLOW 1
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)



set(SYSROOT_DIR "${CMAKE_BINARY_DIR}/_sysroot")
if(NOT EXISTS "/usr/lib/x86_64-linux-gnu/libaom.a" OR NOT EXISTS "/usr/lib/x86_64-linux-gnu/libwebp.a" OR NOT EXISTS "/usr/lib/x86_64-linux-gnu/libliquid.a" OR NOT EXISTS "/usr/lib/x86_64-linux-gnu/libfftw3f.a")
    find_program(APT_GET_EXEC apt-get)
    find_program(DPKG_DEB_EXEC dpkg-deb)
    if(APT_GET_EXEC AND DPKG_DEB_EXEC AND NOT EXISTS "${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu/libaom.a")
        message(STATUS "Populating local build/_sysroot with static development archives (.a)...")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_sysroot_debs" "${SYSROOT_DIR}")
        execute_process(
            COMMAND ${APT_GET_EXEC} download libwebp-dev libsharpyuv-dev libaom-dev libliquid-dev libfftw3-dev
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/_sysroot_debs"
            RESULT_VARIABLE APT_DL_RES
        )
        if(APT_DL_RES EQUAL 0)
            file(GLOB DEB_FILES "${CMAKE_BINARY_DIR}/_sysroot_debs/*.deb")
            foreach(DEB_FILE ${DEB_FILES})
                execute_process(COMMAND ${DPKG_DEB_EXEC} -x "${DEB_FILE}" "${SYSROOT_DIR}")
            endforeach()
        endif()
        file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/_sysroot_debs")
    endif()
endif()

if(EXISTS "${SYSROOT_DIR}/usr/include")
    include_directories("${SYSROOT_DIR}/usr/include")
endif()

# Resolve static WebP + SharpYUV
find_path(WEBP_INCLUDE_DIR webp/encode.h PATHS "/usr/include" "${SYSROOT_DIR}/usr/include")
find_library(WEBP_STATIC_LIB NAMES libwebp.a PATHS "/usr/lib/x86_64-linux-gnu" "/usr/lib" "${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
find_library(SHARPYUV_STATIC_LIB NAMES libsharpyuv.a PATHS "/usr/lib/x86_64-linux-gnu" "/usr/lib" "${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
if(WEBP_INCLUDE_DIR AND WEBP_STATIC_LIB AND SHARPYUV_STATIC_LIB)
    set(WEBP_LIBRARIES ${WEBP_STATIC_LIB} ${SHARPYUV_STATIC_LIB})
else()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_ANIM_UTILS OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_CWEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_DWEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_GIF2WEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_IMG2WEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_VWEBP OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_WEBPINFO OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_WEBPMUX OFF CACHE BOOL "" FORCE)
    set(WEBP_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        libwebp
        GIT_REPOSITORY https://github.com/webmproject/libwebp.git
        GIT_SHALLOW 1
        GIT_TAG v1.5.0
    )
    FetchContent_MakeAvailable(libwebp)
    set(WEBP_LIBRARIES webp)
    set(WEBP_INCLUDE_DIR ${libwebp_SOURCE_DIR}/src)
endif()

# Resolve static AOM + build static libavif via FetchContent
find_path(AOM_INCLUDE_DIR aom/aom_encoder.h PATHS "/usr/include" "${SYSROOT_DIR}/usr/include")
find_library(AOM_STATIC_LIB NAMES libaom.a PATHS "/usr/lib/x86_64-linux-gnu" "/usr/lib" "${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
if(AOM_INCLUDE_DIR AND AOM_STATIC_LIB)
    if(NOT TARGET aom)
        add_library(aom STATIC IMPORTED GLOBAL)
        set_target_properties(aom PROPERTIES
            IMPORTED_LOCATION "${AOM_STATIC_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${AOM_INCLUDE_DIR}"
        )
    endif()
    set(AVIF_CODEC_AOM SYSTEM CACHE STRING "" FORCE)
else()
    set(AVIF_CODEC_AOM LOCAL CACHE STRING "" FORCE)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(AVIF_LIBYUV OFF CACHE STRING "" FORCE)
set(AVIF_LIBSHARPYUV OFF CACHE STRING "" FORCE)
set(AVIF_CODEC_AOM_DECODE OFF CACHE BOOL "" FORCE)
set(AVIF_CODEC_AOM_ENCODE ON CACHE BOOL "" FORCE)
set(AVIF_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(AVIF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libavif
    GIT_REPOSITORY https://github.com/AOMediaCodec/libavif.git
    GIT_SHALLOW 1
    GIT_TAG v1.2.1
)
FetchContent_MakeAvailable(libavif)
set(AVIF_LIBRARIES avif)
set(AVIF_INCLUDE_DIR ${libavif_SOURCE_DIR}/include)

# Resolve static liquid-dsp (+ static libfftw3f.a used by liquid's fir/fft symbols)
find_library(LIQUID_STATIC_LIB NAMES libliquid.a liquid PATHS "/usr/lib/x86_64-linux-gnu" "/usr/lib" "${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
find_library(FFTW3F_STATIC_LIB NAMES libfftw3f.a PATHS "/usr/lib/x86_64-linux-gnu" "/usr/lib" "${SYSROOT_DIR}/usr/lib/x86_64-linux-gnu")
if(LIQUID_STATIC_LIB AND FFTW3F_STATIC_LIB)
    set(LIQUID_LIBRARY ${LIQUID_STATIC_LIB} ${FFTW3F_STATIC_LIB})
elseif(LIQUID_STATIC_LIB)
    set(LIQUID_LIBRARY ${LIQUID_STATIC_LIB})
else()
    set(LIQUID_LIBRARY liquid)
endif()

if(DOWNLOAD_DEPS_ONLY)
    message(STATUS "Downloaded dependencies successfully. Stopping before build targets.")
    return()
endif()

find_package(OpenMP)

function(configure_dsp_cli_target target_name)
    set(USE_STATIC_LINK ${DSP_TOOLS_STATIC})
    if(target_name STREQUAL "dsp_tests" AND DSP_TOOLS_COVERAGE)
        set(USE_STATIC_LINK OFF)
    endif()
    if(OpenMP_CXX_FOUND)
        if(USE_STATIC_LINK)
            target_compile_options(${target_name} PRIVATE ${OpenMP_CXX_FLAGS})
            target_link_options(${target_name} PRIVATE -static ${OpenMP_CXX_FLAGS} -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)
        else()
            target_link_libraries(${target_name} PUBLIC OpenMP::OpenMP_CXX)
        endif()
    else()
        if(USE_STATIC_LINK)
            target_link_options(${target_name} PRIVATE -static)
        endif()
    endif()
endfunction()

add_executable(dsp_plotter
    main.cpp
    dsp_engine.cpp
    plot_generator.cpp
)
configure_dsp_cli_target(dsp_plotter)

target_link_libraries(dsp_plotter
  PRIVATE
    kfr
    kfr_dft
    kfr_io
    kfr_dsp
    CLI11::CLI11
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${WEBP_LIBRARIES}
    ${AVIF_LIBRARIES}
)

target_compile_options(dsp_plotter PRIVATE -Wno-stringop-overflow)
target_include_directories(dsp_plotter PRIVATE ${stb_SOURCE_DIR} ${WEBP_INCLUDE_DIR} ${AVIF_INCLUDE_DIR})

add_executable(dsp_resample
    dsp_resample.cpp
)
configure_dsp_cli_target(dsp_resample)

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
configure_dsp_cli_target(dsp_filter)
target_link_libraries(dsp_filter PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_filter PRIVATE -Wno-stringop-overflow)

add_executable(dsp_tuner dsp_tuner.cpp dsp_tuner_lib.cpp)
configure_dsp_cli_target(dsp_tuner)
target_link_libraries(dsp_tuner PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_tuner PRIVATE -Wno-stringop-overflow)

add_executable(dsp_format dsp_format.cpp)
configure_dsp_cli_target(dsp_format)
target_link_libraries(dsp_format PRIVATE kfr kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_format PRIVATE -Wno-stringop-overflow)

add_executable(dsp_convert dsp_convert.cpp dsp_convert_lib.cpp)
configure_dsp_cli_target(dsp_convert)
target_link_libraries(dsp_convert PRIVATE kfr kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_convert PRIVATE -Wno-stringop-overflow)

add_executable(dsp_whitener dsp_whitener.cpp)
configure_dsp_cli_target(dsp_whitener)
target_link_libraries(dsp_whitener PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_whitener PRIVATE -Wno-stringop-overflow)

add_executable(dsp_fft dsp_fft.cpp dsp_engine.cpp)
configure_dsp_cli_target(dsp_fft)
target_link_libraries(dsp_fft PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_fft PRIVATE -Wno-stringop-overflow)

add_executable(dsp_psd dsp_psd.cpp dsp_engine.cpp)
configure_dsp_cli_target(dsp_psd)
target_link_libraries(dsp_psd PRIVATE kfr kfr_dft kfr_dsp kfr_io CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_psd PRIVATE -Wno-stringop-overflow)

add_executable(dsp_demod dsp_demod.cpp dsp_demod_lib.cpp dsp_tuner_lib.cpp)
configure_dsp_cli_target(dsp_demod)
target_link_libraries(dsp_demod PRIVATE kfr kfr_dft kfr_dsp kfr_io ${LIQUID_LIBRARY} CLI11::CLI11 spdlog::spdlog)
target_compile_options(dsp_demod PRIVATE -Wno-stringop-overflow)

# Python bindings
pybind11_add_module(dsp_plotter_py NO_EXTRAS python_bindings.cpp dsp_engine.cpp plot_generator.cpp dsp_convert_lib.cpp dsp_tuner_lib.cpp dsp_time_domain_lib.cpp dsp_demod_lib.cpp dsp_filter.cpp dsp_resample.cpp dsp_whitener.cpp)
if((CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
    target_compile_options(dsp_plotter_py PRIVATE "-flto=${HALF_CPUS}")
    target_link_options(dsp_plotter_py PRIVATE "-flto=${HALF_CPUS}")
endif()
if(DSP_TOOLS_STATIC AND NOT DSP_TOOLS_COVERAGE)
    target_link_options(dsp_plotter_py PRIVATE -static-libstdc++ -static-libgcc)
endif()
target_compile_definitions(dsp_plotter_py PRIVATE DSP_TOOLS_PYTHON_MODULE=1)
target_link_libraries(dsp_plotter_py PRIVATE kfr kfr_dft kfr_io kfr_dsp ${LIQUID_LIBRARY} spdlog::spdlog ${WEBP_LIBRARIES} ${AVIF_LIBRARIES})
if(OpenMP_CXX_FOUND)
    target_link_libraries(dsp_plotter_py PRIVATE OpenMP::OpenMP_CXX)
endif()
target_compile_options(dsp_plotter_py PRIVATE -Wno-stringop-overflow)
target_include_directories(dsp_plotter_py PRIVATE ${stb_SOURCE_DIR} ${WEBP_INCLUDE_DIR} ${AVIF_INCLUDE_DIR})

# ----------------- TESTS -----------------
enable_testing()
add_executable(dsp_tests
    tests/test_main.cpp
    tests/test_bluefile_io.cpp
    tests/test_whitener.cpp
    tests/test_filter.cpp
    tests/test_tuner.cpp
    tests/test_resample.cpp
    tests/test_format.cpp
    tests/test_plotter.cpp
    plot_generator.cpp
)
target_compile_definitions(dsp_tests PRIVATE DSP_TOOLS_TEST_MODE=1)
configure_dsp_cli_target(dsp_tests)
target_link_libraries(dsp_tests PRIVATE Catch2::Catch2WithMain kfr kfr_dft kfr_dsp kfr_io spdlog::spdlog CLI11::CLI11 ${WEBP_LIBRARIES} ${AVIF_LIBRARIES})
target_compile_options(dsp_tests PRIVATE -Wno-stringop-overflow)
target_include_directories(dsp_tests PRIVATE ${stb_SOURCE_DIR} ${WEBP_INCLUDE_DIR} ${AVIF_INCLUDE_DIR})
catch_discover_tests(dsp_tests)

if(DSP_TOOLS_COVERAGE)
    target_compile_options(dsp_tests PRIVATE --coverage -O0 -g)
    target_link_options(dsp_tests PRIVATE --coverage)
    add_custom_target(coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND gcovr -r ${CMAKE_SOURCE_DIR}
            --cobertura coverage.xml
            --txt coverage.txt
            --filter "${CMAKE_SOURCE_DIR}/"
            --exclude ".*_deps/.*"
            --exclude ".*tests/.*"
            --exclude ".*stb_truetype\\.h"
            --exclude ".*embedded_font\\.h"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating coverage report..."
    )
endif()
