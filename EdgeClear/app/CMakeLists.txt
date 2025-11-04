cmake_minimum_required(VERSION 3.22.1)

project(edgeclear-native C CXX)

# C++20 Standard (Constitution requirement - Section VI)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C11 Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags for real-time audio performance
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fno-exceptions -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

# ARM NEON optimizations
if(ANDROID_ABI MATCHES "^armeabi-v7a")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon")
    add_definitions(-DUSE_NEON=1)
elseif(ANDROID_ABI MATCHES "^arm64-v8a")
    # NEON is mandatory on arm64-v8a, no flag needed
    add_definitions(-DUSE_NEON=1)
endif()

# Release optimizations
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG -flto")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DNDEBUG -flto")

# Debug settings
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -DDEBUG")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -DDEBUG")

# Find required libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(aaudio-lib aaudio)

# Import TensorFlow Lite via prefab (optional for now)
find_package(tensorflowlite CONFIG)

# pffft library (lightweight FFT)
add_library(pffft STATIC
    src/main/cpp/third_party/pffft/pffft.c
)
target_include_directories(pffft PUBLIC
    src/main/cpp/third_party/pffft
)

# Main native library
add_library(edgeclear-native SHARED
    # JNI Bridge
    src/main/cpp/jni_bridge.cpp

    # Real-time utilities
    src/main/cpp/rt/ring_buffer.cpp
    src/main/cpp/rt/perf_counters.cpp
    src/main/cpp/rt/frame_buffer.cpp

    # DSP modules
    src/main/cpp/dsp/cola_windows.cpp
    src/main/cpp/dsp/stft.cpp
    src/main/cpp/dsp/istft.cpp
    src/main/cpp/dsp/resampler.cpp

    # AEC modules (Phase 4: M2)
    src/main/cpp/aec/aec.cpp
    src/main/cpp/aec/dtd.cpp
    src/main/cpp/aec/res.cpp

    # ML inference (Phase 5: M3) - TEMPORARILY DISABLED until TensorFlow Lite prefab is working
    # src/main/cpp/ml/tflite_loader.cpp
    # src/main/cpp/ml/denoiser_inference.cpp

    # Audio I/O
    src/main/cpp/audio_io/aaudio_capture.cpp
    src/main/cpp/audio_io/aaudio_render.cpp
    src/main/cpp/audio_io/audio_callback.cpp
    src/main/cpp/audio_io/latency_probe.cpp
    src/main/cpp/audio_io/wav_writer.cpp  # Phase 6: A/B Recording

    # AV-VAD (Phase 9: optional A+ feature)
    # src/main/cpp/av_vad/audio_vad.cpp
    # src/main/cpp/av_vad/av_vad_fusion.cpp

    # Pipeline orchestration
    # src/main/cpp/pipeline/session_state.cpp  # Phase 7: Presets
    src/main/cpp/pipeline/session_manager.cpp
    src/main/cpp/pipeline/dsp_worker.cpp
    # src/main/cpp/pipeline/passthrough_pipeline.cpp  # Replaced by inline in dsp_worker
    src/main/cpp/pipeline/metrics_accumulator.cpp
)

target_include_directories(edgeclear-native PRIVATE
    src/main/cpp
    src/main/cpp/rt
    src/main/cpp/dsp
    src/main/cpp/aec
    src/main/cpp/ml
    src/main/cpp/audio_io
    src/main/cpp/av_vad
    src/main/cpp/pipeline
    src/main/cpp/third_party/pffft
)

target_link_libraries(edgeclear-native
    ${log-lib}
    ${android-lib}
    ${aaudio-lib}
    pffft
)

# Link TensorFlow Lite if available
if(tensorflowlite_FOUND)
    target_link_libraries(edgeclear-native tensorflowlite::tensorflowlite)
    target_compile_definitions(edgeclear-native PRIVATE HAVE_TFLITE=1)
else()
    message(WARNING "TensorFlow Lite not found via prefab - ML inference will be disabled")
endif()

# Enable testing (optional, for C++ unit tests)
option(BUILD_TESTING "Build C++ unit tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(src/main/cpp/test)
endif()
