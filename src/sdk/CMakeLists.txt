# yave_sdk : 字幕エフェクトプラグイン作者向け公開ヘッダのみの INTERFACE ターゲット。
# 誰にも依存しない。yave_subtitle と外部プラグインが include する。
add_library(yave_sdk INTERFACE)
target_include_directories(yave_sdk INTERFACE ${CMAKE_SOURCE_DIR}/include)
