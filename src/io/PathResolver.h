#pragma once

#include <QDir>
#include <QString>

namespace yave::io {

/// 相対 <-> 絶対パス変換。
///
/// すべてのパスはプロジェクトファイル相対 ('/' 区切り) で保存する。
/// 相対化できない場所 (別ドライブ等) は絶対パスのまま保存し、
/// 保存時に警告 + 「アセットを収集」を提案する (3.4 参照)。
class PathResolver
{
public:
    explicit PathResolver(const QString& projectFilePath);

    /// 絶対パス -> プロジェクト相対パス。
    /// 相対化できない場合 (別ドライブ / プロジェクト外) は絶対パスを返す。
    QString toRelative(const QString& absolutePath, bool* relativizedOut = nullptr) const;

    /// 相対パス -> 絶対パス。
    /// 探索順: プロジェクトフォルダ相対 -> assets/ 直下 (ファイル名一致)。
    /// 見つからない場合は候補パスを返す (resolvedOut == false)。読み込みは失敗させない。
    QString toAbsolute(const QString& relativePath, bool* resolvedOut = nullptr) const;

    /// パス区切りの正規化。JSON には常に '/' で保存する。
    static QString normalizeSeparators(const QString& p);

private:
    QDir projectDir_;
};

} // namespace yave::io
