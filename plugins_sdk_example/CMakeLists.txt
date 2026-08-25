# 字幕エフェクトプラグインのサンプル実装 (GlitchEffect)。
# yave_sdk ヘッダのみに依存し、ホストと C ABI で結合する。

set(GLITCH_SOURCES GlitchEffect.cpp)

add_library(GlitchEffect MODULE ${GLITCH_SOURCES})
yave_apply_common_settings(GlitchEffect)
target_link_libraries(GlitchEffect PRIVATE Qt6::Core Qt6::Gui yave_sdk)
target_compile_definitions(GlitchEffect PRIVATE YAVE_SDK_LIBRARY)

set_target_properties(GlitchEffect PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins")
