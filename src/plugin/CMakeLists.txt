set(YAVE_PLUGIN_COMMON_SOURCES
    PluginManager.h
    PluginManager.cpp
    PluginDescriptor.h
    PluginWindow.h
    PluginWindow.cpp
    SubtitleEffectRegistry.h
    SubtitleEffectRegistry.cpp

    subtitle/SubtitleEffectLoader.h
    subtitle/SubtitleEffectLoader.cpp
    subtitle/BuiltinEffectFactory.h

    vst3/Vst3HostStub.h
)

add_library(yave_plugin STATIC ${YAVE_PLUGIN_COMMON_SOURCES})
yave_apply_common_settings(yave_plugin)
target_link_libraries(yave_plugin
    PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets yave_core yave_audio yave_subtitle)

target_include_directories(yave_plugin PUBLIC ${CMAKE_SOURCE_DIR}/include)

# ---------------------------------------------------------------------------
# AviUtl プラグインホスト  ★ Windows 専用
#
#   - macOS / Linux ではソースファイル自体をターゲットに追加しない。
#   - YAVE_ENABLE_AVIUTL は OFF にできる (Windows でも無効化可能)。
#   - 有効時のみ YAVE_ENABLE_AVIUTL コンパイル定義が入る。
# ---------------------------------------------------------------------------
if(WIN32 AND YAVE_ENABLE_AVIUTL AND EXISTS ${CMAKE_SOURCE_DIR}/third_party/aviutl_sdk/filter.h)
    message(STATUS "AviUtl plugin host: ENABLED (Windows x64)")
    target_sources(yave_plugin PRIVATE
        aviutl/AviUtlHost.cpp
    )
    target_include_directories(yave_plugin PRIVATE ${CMAKE_SOURCE_DIR}/third_party/aviutl_sdk)
    target_compile_definitions(yave_plugin PUBLIC YAVE_ENABLE_AVIUTL=1)
else()
    message(STATUS "AviUtl plugin host: DISABLED")
endif()

if(YAVE_ENABLE_VST3)
    # VST3 SDK は FetchContent で取得する (third_party/CMakeLists.txt 参照)。
    target_compile_definitions(yave_plugin PUBLIC YAVE_ENABLE_VST3=1)
    target_link_libraries(yave_plugin PRIVATE sdk_hosting base pluginterfaces)
endif()
