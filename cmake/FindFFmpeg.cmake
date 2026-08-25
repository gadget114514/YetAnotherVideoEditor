# Locate FFmpeg libraries and headers.
#
# Search order of the install prefix:
#   1. -DFFMPEG_ROOT=<path>          (explicit cache entry)
#   2. $ENV{FFMPEG_ROOT}             (per-find HINTS)
#   3. third_party/ffmpeg/<dist>     (vendored copy, auto-detected)
#   4. Homebrew prefixes             (macOS only)
#
# Imported targets (when found):
#   FFmpeg::avcodec FFmpeg::avformat FFmpeg::avutil
#   FFmpeg::swscale FFmpeg::swresample FFmpeg::avfilter
#
# Result variables:
#   FFmpeg_FOUND, FFMPEG_INCLUDE_DIRS, FFMPEG_LIBRARIES, FFMPEG_VERSION

include(FindPackageHandleStandardArgs)

set(_ffmpeg_components avcodec avformat avutil swscale swresample avfilter)
set(_ffmpeg_required_vars)

if(NOT FFMPEG_ROOT AND NOT DEFINED ENV{FFMPEG_ROOT})
    # 同梱ビルド (third_party/ffmpeg/<dist>) を自動検出する。
    # 複数ある場合はツールチェーンに合う import library 形式を優先する。
    file(GLOB _ffmpeg_vendored
        LIST_DIRECTORIES TRUE
        "${CMAKE_CURRENT_LIST_DIR}/../third_party/ffmpeg/*")
    set(_ffmpeg_candidates)
    foreach(_d ${_ffmpeg_vendored})
        if(EXISTS "${_d}/include/libavcodec/avcodec.h")
            list(APPEND _ffmpeg_candidates "${_d}")
        endif()
    endforeach()
    if(_ffmpeg_candidates)
        set(_ffmpeg_pick)
        foreach(_d ${_ffmpeg_candidates})
            if(MSVC AND EXISTS "${_d}/lib/avutil.lib")
                set(_ffmpeg_pick "${_d}")
                break()
            endif()
            if((MINGW OR WIN32) AND EXISTS "${_d}/lib/libavutil.dll.a")
                set(_ffmpeg_pick "${_d}")
                break()
            endif()
        endforeach()
        if(NOT _ffmpeg_pick)
            list(GET _ffmpeg_candidates 0 _ffmpeg_pick)
        endif()
        get_filename_component(_ffmpeg_pick "${_ffmpeg_pick}" ABSOLUTE)
        set(FFMPEG_ROOT "${_ffmpeg_pick}" CACHE PATH
            "FFmpeg install prefix (vendored under third_party/ffmpeg)")
        message(STATUS "Using vendored FFmpeg: ${FFMPEG_ROOT}")
    endif()
    unset(_ffmpeg_vendored)
    unset(_ffmpeg_candidates)
    unset(_ffmpeg_pick)
endif()

if(NOT FFMPEG_ROOT)
    if(APPLE)
        foreach(_p /opt/homebrew /usr/local)
            if(EXISTS "${_p}/include/libavcodec/avcodec.h")
                set(FFMPEG_ROOT "${_p}" CACHE PATH "FFmpeg install prefix")
                break()
            endif()
        endforeach()
    endif()
endif()

foreach(_comp ${_ffmpeg_components})
    string(TOUPPER ${_comp} _UCOMP)

    find_path(FFMPEG_${_UCOMP}_INCLUDE_DIR
        NAMES lib${_comp}/${_comp}.h
        HINTS ${FFMPEG_ROOT} ENV FFMPEG_ROOT
        PATH_SUFFIXES include
    )

    # Library names differ across platforms: libavcodec.dll.a / avcodec.lib / libavcodec.so
    find_library(FFMPEG_${_UCOMP}_LIBRARY
        NAMES ${_comp} lib${_comp}
        HINTS ${FFMPEG_ROOT} ENV FFMPEG_ROOT
        PATH_SUFFIXES bin lib
    )

    if(FFMPEG_${_UCOMP}_INCLUDE_DIR AND FFMPEG_${_UCOMP}_LIBRARY)
        if(NOT TARGET FFmpeg::${_comp})
            add_library(FFmpeg::${_comp} UNKNOWN IMPORTED)
            set_target_properties(FFmpeg::${_comp} PROPERTIES
                IMPORTED_LOCATION "${FFMPEG_${_UCOMP}_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_${_UCOMP}_INCLUDE_DIR}")
        endif()
        list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${_UCOMP}_LIBRARY})
    else()
        set(FFmpeg_FOUND FALSE)
    endif()

    list(APPEND _ffmpeg_required_vars FFMPEG_${_UCOMP}_INCLUDE_DIR FFMPEG_${_UCOMP}_LIBRARY)
endforeach()

if(NOT FFMPEG_INCLUDE_DIRS AND FFMPEG_avutil_INCLUDE_DIR)
    set(FFMPEG_INCLUDE_DIRS "${FFMPEG_avutil_INCLUDE_DIR}")
endif()

find_package_handle_standard_args(FFmpeg
    REQUIRED_VARS ${_ffmpeg_required_vars}
    VERSION_VAR FFMPEG_VERSION
)
