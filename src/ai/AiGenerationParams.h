#pragma once

#include "../core/Rational.h"
#include "../core/TimeRange.h"

#include <QJsonObject>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QUuid>

#include <optional>
#include <vector>

namespace yave::ai {

/// 何を生成するか
enum class GenerationKind
{
    Video,           ///< 動画
    Audio,           ///< 音声 (ナレーション / 効果音 / BGM)
    Subtitle,        ///< 字幕 (書き起こし / スクリプト生成 / 翻訳)
    Mask,            ///< マスク画像シーケンス
    EffectMetadata,  ///< エフェクトパラメータ (自動カラーグレード等)
    Image            ///< 静止画
};

/// 動画生成のサブモード
enum class VideoGenMode
{
    TextToVideo,     ///< T2V : プロンプトのみから生成
    ImageToVideo,    ///< I2V : 参照画像をベースに生成
    VideoToVideo     ///< V2V : 既存レイヤーの映像をベースにスタイル変換 / アニメ化
};

/// I2V における参照フレームの与え方
enum class I2VReferenceMode
{
    StartFrameOnly,  ///< 開始時点のみ参照 (そこから動き出す)
    EndFrameOnly,    ///< 終了時点のみ参照 (そこへ収束する)
    BothEnds         ///< 両端参照 = 前後キーフレーム間の補間生成
};

enum class AudioGenMode    { Narration, SoundEffect, Bgm, VoiceConversion };
enum class SubtitleGenMode { SpeechToText, ScriptFromPrompt, Translate };

/// 参照画像の指定方法
struct ImageReference
{
    enum class Source { FilePath, TimelineFrame };

    Source  source = Source::FilePath;
    QString filePath;                ///< Source::FilePath 用。プロジェクト相対パス
    QUuid   sourceTrackId;           ///< Source::TimelineFrame 用。空なら全レイヤー合成
    int64_t sourceFrame = 0;         ///< Source::TimelineFrame 用
    double  strength    = 1.0;       ///< 参照の効き具合 0..1

    QJsonObject toJson() const;
    static ImageReference fromJson(const QJsonObject& o);

    bool operator==(const ImageReference& o) const
    {
        return source == o.source && filePath == o.filePath
            && sourceTrackId == o.sourceTrackId && sourceFrame == o.sourceFrame
            && strength == o.strength;
    }
};

/// V2V のソース映像指定
struct VideoReference
{
    QUuid     sourceTrackId;
    TimeRange sourceRange;              ///< 空なら生成区間と同じ
    double    denoiseStrength = 0.6;    ///< 元映像をどれだけ残すか (0=完全保持, 1=完全生成)

    QJsonObject toJson() const;
    static VideoReference fromJson(const QJsonObject& o);

    bool operator==(const VideoReference& o) const
    {
        return sourceTrackId == o.sourceTrackId && sourceRange == o.sourceRange
            && denoiseStrength == o.denoiseStrength;
    }
};

/// 生成の全パラメータ。これがそのままプロジェクト JSON に永続化される。
struct AiGenerationParams
{
    // ---------------- 共通 ----------------
    GenerationKind kind = GenerationKind::Video;
    QUuid          targetTrackId;      ///< 生成物を置くトラック
    TimeRange      range;              ///< In / Out (タイムラインのフレーム単位)
    QString        modelId;            ///< "wan2.2-i2v-14b" 等
    QString        providerId;         ///< 空なら ProviderRegistry が自動選択
    QString        prompt;
    QString        negativePrompt;
    int64_t        seed = -1;          ///< -1 = ランダム。生成後に実値を書き戻す
    int            steps = 30;
    double         guidanceScale = 7.5;
    QJsonObject    extraParams;        ///< モデル固有の追加パラメータ

    // ---------------- Video ----------------
    VideoGenMode                  videoMode  = VideoGenMode::TextToVideo;
    I2VReferenceMode              i2vRefMode = I2VReferenceMode::StartFrameOnly;
    std::optional<ImageReference> startReference;
    std::optional<ImageReference> endReference;
    std::optional<VideoReference> videoReference;
    QSize                         outputResolution{1280, 720};
    Rational                      outputFrameRate = timebase::Fps30;
    bool                          loopSeamless = false;

    // ---------------- Audio ----------------
    AudioGenMode audioMode = AudioGenMode::Narration;
    QString      voiceId;
    double       speakingRate = 1.0;
    double       pitch        = 0.0;
    QString      referenceAudioPath;      ///< ボイスクローン用 (プロジェクト相対)
    int          audioSampleRate = 48000;
    int          audioChannels   = 2;
    double       targetLufs      = -16.0; ///< ラウドネス正規化目標

    // ---------------- Subtitle ----------------
    SubtitleGenMode subtitleMode = SubtitleGenMode::SpeechToText;
    QUuid           sourceAudioTrackId;
    QString         language;             ///< "ja" / "en" / "auto"
    QString         targetLanguage;
    bool            wordLevelTimestamps = true;
    QString         subtitleStylePresetId = QStringLiteral("default");
    int             maxCharsPerCue = 0;   ///< 0 = 言語ごとの既定値を使う
    int64_t         minCueDurationFrames = 0;

    // ---------------- Mask ----------------
    QUuid                maskSourceTrackId;
    QString              maskTargetDescription;   ///< "person" / "sky" 等
    std::vector<QPointF> maskHintPoints;          ///< SAM 用のクリックヒント (正規化座標)
    bool                 maskTrackAcrossFrames = true;
    bool                 maskFeather = true;

    // ---------------- 出力の扱い ----------------
    bool replaceExistingClips = false;
    bool createNewTrack       = false;

    // ---------------- シリアライズ / 検証 / ハッシュ ----------------
    QJsonObject toJson() const;
    static AiGenerationParams fromJson(const QJsonObject& o);

    struct ValidationResult
    {
        bool    ok = true;
        QString errorKey;      ///< "error.ai.missingStartRef" 等の翻訳キー
    };
    ValidationResult validate() const;

    /// キャッシュキー計算用の正規化ハッシュ。配置先 (targetTrackId 等) は含めない。
    QByteArray contentHash() const;
};

} // namespace yave::ai
