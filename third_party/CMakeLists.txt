# third_party
#   ffmpeg      : 同梱 shared ビルド。cmake/FindFFmpeg.cmake が自動検出する
#                 (明示指定したい場合は -DFFMPEG_ROOT=<path> か環境変数 FFMPEG_ROOT)
#   vst3sdk     : FetchContent で取得 (コミットしない)
#   aviutl_sdk  : ヘッダのみ同梱

if(YAVE_ENABLE_VST3)
    include(FetchContent)
    FetchContent_Declare(vst3sdk
        GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
        GIT_TAG        v3.7.9_build_61_10-2024-01-16
        GIT_SHALLOW    TRUE
    )
    set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(vst3sdk)
    message(STATUS "VST3 SDK fetched: ${vst3sdk_SOURCE_DIR}")
else()
    message(STATUS "VST3 SDK disabled (YAVE_ENABLE_VST3=OFF)")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/aviutl_sdk/filter.h)
    message(STATUS "AviUtl SDK headers found")
else()
    message(STATUS "AviUtl SDK headers missing; AviUtl host stays disabled")
endif()
