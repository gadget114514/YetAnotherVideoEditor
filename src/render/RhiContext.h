#pragma once

#include <QSize>
#include <QString>

namespace yave::render {

/// QRhi の生成とバックエンド選択。
///
/// バックエンド:
///   Windows : D3D11 (既定) / D3D12
///   macOS   : Metal
///
/// QQuickRhiItem ベースのプレビューでは、Qt Quick シーングラフが持つ QRhi を
/// 共有する。スタンドアロン (書き出し) 用途では createStandaloneRhi() で自前生成する。
class RhiContext
{
public:
    /// 起動シーケンス (1.5) で呼ぶ。プラットフォーム既定の RHI バックエンドを
    /// QSG_RHI_BACKEND 環境変数へ設定する。ユーザー指定があれば尊重する。
    static void selectBackend();

    static QString defaultBackendName();

    /// 書き出し用のスタンドアロン QRhi を作る (プレビューと GPU を共有しない)。
    /// 失敗時は nullptr。戻り値の所有は呼び出し側 (QRhi*)。
    static void* createStandaloneRhi();
};

} // namespace yave::render
