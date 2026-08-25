cmake_minimum_required(VERSION 3.24)
project(YAVE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PlatformConfig)
include(YaveDeploy)

# ---- プラットフォーム既定値 ---------------------------------------------
if(WIN32)
    set(YAVE_AVIUTL_DEFAULT ON)
    set(YAVE_QSV_DEFAULT    ON)
    set(YAVE_CUDA_DEFAULT   ON)
    set(YAVE_VT_DEFAULT     OFF)
elseif(APPLE)
    set(YAVE_AVIUTL_DEFAULT OFF)
    set(YAVE_QSV_DEFAULT    OFF)
    set(YAVE_CUDA_DEFAULT   OFF)
    set(YAVE_VT_DEFAULT     ON)
else()
    set(YAVE_AVIUTL_DEFAULT OFF)
    set(YAVE_QSV_DEFAULT    OFF)
    set(YAVE_CUDA_DEFAULT   OFF)
    set(YAVE_VT_DEFAULT     OFF)
endif()

# AviUtl SDK ヘッダが同梱されていない場合は既定で無効化する
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/aviutl_sdk/filter.h")
    set(YAVE_AVIUTL_DEFAULT OFF)
endif()

option(YAVE_ENABLE_FFMPEG       "Build FFmpeg-backed media engine"              ON)
option(YAVE_ENABLE_AVIUTL       "Enable AviUtl x64 plugin host (Windows only)"  ${YAVE_AVIUTL_DEFAULT})
option(YAVE_ENABLE_QSV          "Enable Intel QuickSync"                        ${YAVE_QSV_DEFAULT})
option(YAVE_ENABLE_CUDA         "Enable NVDEC/NVENC"                            ${YAVE_CUDA_DEFAULT})
option(YAVE_ENABLE_VIDEOTOOLBOX "Enable VideoToolbox"                           ${YAVE_VT_DEFAULT})
option(YAVE_ENABLE_ONNX_LOCAL   "Enable local ONNX Runtime inference"           OFF)
option(YAVE_ENABLE_VST3         "Enable VST3 plugin host (fetches VST3 SDK)"    OFF)
option(YAVE_BUILD_TESTS         "Build unit tests"                              ON)

# 非 Windows で AviUtl を要求されたら明示的に落とす(黙って無視しない)
if(YAVE_ENABLE_AVIUTL AND NOT WIN32)
    message(FATAL_ERROR
        "YAVE_ENABLE_AVIUTL is only supported on Windows. Configure with -DYAVE_ENABLE_AVIUTL=OFF.")
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickControls2 Qml Network Concurrent Test ShaderTools LinguistTools)
qt_standard_project_setup(REQUIRES 6.6)

if(YAVE_ENABLE_FFMPEG)
    find_package(FFmpeg COMPONENTS avcodec avformat avutil swscale swresample avfilter)
    if(NOT FFmpeg_FOUND)
        message(WARNING
            "FFmpeg was not found. The media engine will be built in stub mode "
            "(decoding/encoding unavailable). Install FFmpeg dev packages or "
            "configure with -DYAVE_ENABLE_FFMPEG=OFF to silence this warning.")
        set(YAVE_ENABLE_FFMPEG OFF)
    endif()
endif()

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(plugins_sdk_example)
if(YAVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
