#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>

#include <memory>

namespace yave {
class Asset;
class Clip;
class Project;
class Timeline;
class Track;

namespace ai {
struct AiGenerationParams;
class AiGenerationTask;
class GeneratedAsset;
enum class TaskState;
} // namespace ai

namespace subtitle {
class SubtitleClip;
struct SubtitleEffectInstance;
} // namespace subtitle
} // namespace yave

namespace yave::io {

struct SaveOptions
{
    bool indented               = true;   ///< false なら Compact
    bool collectGeneratedAssets = false;  ///< AI 生成物を assets/generated/ へコピー
    bool collectSourceAssets    = false;  ///< 素材を assets/ へコピー
    bool embedThumbnails        = false;  ///< サムネイルを Base64 で埋め込む
    bool omitDefaultValues      = true;   ///< 既定値と同じフィールドを省略してサイズを削減
};

struct LoadResult
{
    bool        ok = false;
    QString     errorMessage;
    QStringList warnings;                  ///< 未解決アセット / 未インストールプラグイン等
    int         loadedSchemaVersion = 0;
    bool        migrated = false;
    QStringList missingAssetPaths;
    QStringList missingPluginIds;
};

/// JSON によるプロジェクトの保存・読み込み管理。
///
/// 設計方針:
///   - 無限レイヤーは "tracks" 配列で表現する。配列順 = Z オーダー。
///   - enum は必ず文字列で保存する (数値だと enum への値追加で壊れる)。
///   - パスは常にプロジェクトファイル相対、区切りは '/'。
///   - 認識できないフィールドは保持して書き戻す (前方互換)。
///   - 保存は QSaveFile による原子的書き込み。
class ProjectSerializer
{
public:
    /// 3 = フィルタ / トランジション / タイトル / ライブラリ (9.11)
    static constexpr int kCurrentSchemaVersion = 3;

    // ================= トップレベル =================

    static bool       save(const Project& project, const QString& path,
                           const SaveOptions& opts, QString* errorOut = nullptr);
    static LoadResult load(Project* project, const QString& path);

    /// 自動保存 (CBOR。速度優先)
    static bool       saveAutosave(const Project& project, const QString& path);
    static LoadResult loadAutosave(Project* project, const QString& path);

    // ================= 個別シリアライズ (単体テスト用に public) =================

    static QJsonObject serializeProject(const Project& p, const SaveOptions& o);
    static QJsonArray  serializeAssets(const Project& p);
    static QJsonArray  serializeSubtitleStylePresets(const Project& p);

    /// 無限レイヤー: Track の動的リストを QJsonArray にする
    static QJsonArray  serializeTracks(const Timeline& tl, const SaveOptions& o);
    static QJsonObject serializeTrack(const Track& t, const SaveOptions& o);

    static QJsonObject serializeClip(const Clip& c, const SaveOptions& o);
    static QJsonObject serializeSubtitleClip(const subtitle::SubtitleClip& c);
    static QJsonArray  serializeSubtitleEffectStack(
                           const std::vector<subtitle::SubtitleEffectInstance>& stack);

    static QJsonObject serializeAiParams(const ai::AiGenerationParams& p);
    static QJsonObject serializeAiTask(const ai::AiGenerationTask& t);

    // ================= 個別デシリアライズ =================

    static bool deserializeProject(Project* p, const QJsonObject& o, LoadResult* r);
    static void deserializeAssets(Project* p, const QJsonArray& arr, LoadResult* r,
                                  const QString& projectFilePath);
    static void deserializeTracks(Timeline* tl, const QJsonArray& arr,
                                  Project* p, LoadResult* r);
    static std::unique_ptr<Track> deserializeTrack(const QJsonObject& o,
                                                   Project* p, LoadResult* r);
    static std::shared_ptr<Clip>  deserializeClip(const QJsonObject& o,
                                                  Project* p, LoadResult* r);
    static std::shared_ptr<subtitle::SubtitleClip>
                                  deserializeSubtitleClip(const QJsonObject& o,
                                                          Project* p, LoadResult* r);
    static std::vector<subtitle::SubtitleEffectInstance>
                                  deserializeSubtitleEffectStack(const QJsonArray& arr,
                                                                 LoadResult* r);
    static ai::AiGenerationParams deserializeAiParamsObject(const QJsonObject& o);

private:
    /// schemaVersion が古い場合にマイグレーションを順次適用する
    static void applyMigrations(QJsonObject& root, int fromVersion, LoadResult* r);

    /// 直近 N 世代のバックアップを回転させる
    static void rotateBackups(const QString& path, int generations);
};

} // namespace yave::io
