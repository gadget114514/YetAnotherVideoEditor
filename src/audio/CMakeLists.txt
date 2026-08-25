set(YAVE_AUDIO_SOURCES
    IAudioDevice.h
    IAudioDevice.cpp
    AudioClock.h
    LockFreeRingBuffer.h
    DelayCompensator.h
    DelayCompensator.cpp
    AudioRenderGraph.h
    AudioRenderGraph.cpp
    AudioRenderEngine.h
    AudioRenderEngine.cpp
    MeterBridge.h
)

add_library(yave_audio STATIC ${YAVE_AUDIO_SOURCES})
yave_apply_common_settings(yave_audio)
target_link_libraries(yave_audio
    PUBLIC yave_core Qt6::Core Qt6::Gui)

# ---- Windows: WASAPI デバイス実装 ---------------------------------------
#   mingw-w64 / MSVC ともに mmdeviceapi.h を持つ。ビルド失敗時は
#   -DYAVE_ENABLE_WASAPI=OFF で無効化できる (NullAudioDevice にフォールバック)。
if(WIN32 AND NOT DEFINED YAVE_ENABLE_WASAPI)
    set(YAVE_ENABLE_WASAPI ON CACHE BOOL "Enable WASAPI audio device (Windows)")
endif()

if(YAVE_ENABLE_WASAPI AND WIN32)
    try_compile(YAVE_WASAPI_COMPILES
        ${CMAKE_CURRENT_BINARY_DIR}/try_wasapi
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../platform/win/WasapiProbe.cpp
        CXX_STANDARD 17)
endif()

if(YAVE_ENABLE_WASAPI AND WIN32)
    target_sources(yave_audio PRIVATE ../platform/win/WasapiDevice.cpp)
    target_compile_definitions(yave_audio PUBLIC YAVE_ENABLE_WASAPI=1 NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        set_source_files_properties(../platform/win/WasapiDevice.cpp
            PROPERTIES COMPILE_OPTIONS "/wd4996")
    else()
        # mingw-w64 では avrt の一部シンボルが未供給の場合があるため、
        # リンクは動的解決に任せる。必要なら -lavrt を追加する。
        target_link_libraries(yave_audio PRIVATE avrt)
    endif()
endif()
