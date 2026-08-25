#pragma once

// VST3 ホストのスタブ宣言 (YAVE_ENABLE_VST3 無効ビルド用)。
//
// VST3 SDK を FetchContent で取得できる環境では third_party/CMakeLists.txt が
// 実装をビルドする。無効な場合も PluginManager::createVst3() のシグネチャを
// 満たすためだけの前方宣言が必要になる。

#include <QString>
#include <memory>

namespace yave::plugin {

class Vst3Host
{
public:
    virtual ~Vst3Host() = default;

    /// プラグインが申告する処理遅延 (サンプル数)。PDC 計算に使う。
    virtual int64_t latencySamples() const = 0;
    virtual bool isEnabled() const = 0;
    virtual QString nativeId() const = 0;
};

} // namespace yave::plugin
