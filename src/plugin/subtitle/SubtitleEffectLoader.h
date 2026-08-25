#pragma once

#include "../../../include/yave/sdk/SubtitleEffectApi.h"
#include "../SubtitleEffectRegistry.h"

#include <QLibrary>

#include <QString>
#include <vector>

namespace yave::plugin {

class LoadedSubtitlePlugin;

/// 外部字幕エフェクト (.dll/.dylib) の C ABI ローダ (6.6.5 参照)。
///
/// エクスポート関数は 1 つだけ:
///   extern "C" const yave::sdk::SubtitleEffectFactoryV1* yaveCreateSubtitleEffectFactory();
///
/// ロードとファクトリ呼び出しは SEH で保護し、不正なプラグイン 1 つで
/// アプリ全体が落ちないようにする。クラッシュしたプラグインはブラックリストへ。
class SubtitleEffectLoader
{
public:
    /// プラグインをロードして Registry へ登録する。失敗時 false。
    static bool load(const QString& path, SubtitleEffectRegistry& registry);

    /// 走査用の簡易チェック (エントリポイント存在 + API バージョン一致)。
    static bool probe(const QString& path);

    /// アンロード直前に factory->shutdown() を呼ぶ。
    static void unloadAll();

private:
    struct Loaded
    {
        QString path;
        std::unique_ptr<QLibrary> library;
        const yave::sdk::SubtitleEffectFactoryV1* factory = nullptr;
    };
    static std::vector<Loaded>& loaded();
};

} // namespace yave::plugin
