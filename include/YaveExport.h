#pragma once

/// DLL エクスポートマクロ。
/// yave は静的リンクを基本としつつ、字幕エフェクトプラグイン SDK 向けに
/// 動的エクスポートも可能にしておく。
#if defined(YAVE_BUILD_SDK_SHARED)
#  if defined(_WIN32)
#    if defined(YAVE_SDK_LIBRARY)
#      define YAVE_PLUGIN_EXPORT __declspec(dllexport)
#    else
#      define YAVE_PLUGIN_EXPORT __declspec(dllimport)
#    endif
#  else
#    if defined(YAVE_SDK_LIBRARY)
#      define YAVE_PLUGIN_EXPORT __attribute__((visibility("default")))
#    else
#      define YAVE_PLUGIN_EXPORT
#    endif
#  endif
#else
#  define YAVE_PLUGIN_EXPORT
#endif
