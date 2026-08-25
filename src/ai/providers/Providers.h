#pragma once

#include "../IGenerationProvider.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace yave::ai {

/// リモート HTTP API 経由の生成プロバイダ。
///
/// 対応想定:
///   - ComfyUI ローカルサーバ (http://127.0.0.1:8188)
///   - OpenAI 互換 API (TTS / STT)
///   - Replicate 等
///
/// 認証情報は QNetworkAccessManager の既定の仕組みに任せ、
/// **プロジェクトファイルへは一切保存しない** (非機能要件 5 参照)。
/// API キーはアプリ固有の永続領域 (QSettings / OS keychain) のみに置く。
class RemoteHttpProvider : public IGenerationProvider
{
public:
    explicit RemoteHttpProvider(const QString& baseUrl = {});

    QString id() const override { return QStringLiteral("remote-http"); }
    QString displayName() const override { return QStringLiteral("Remote HTTP API"); }

    ProviderCapability capabilities() const override
    {
        ProviderCapability c;
        c.video = true;
        c.audio = true;
        c.subtitle = true;
        c.image = true;
        c.mask = false;
        return c;
    }

    /// エンドポイントが設定されていなければ利用不可とみなす
    bool isAvailable(QString* errorOut) const override;

    GenerationOutput run(const GenerationInput& input,
                         const ProgressFn& progress,
                         const CancelCheckFn& cancelled) override;

    void prepareParams(AiGenerationParams&) override {}

    void setBaseUrl(const QString& url) { baseUrl_ = url; }
    QString baseUrl() const { return baseUrl_; }

private:
    QString baseUrl_;
};

/// sidecar 子プロセス経由の生成プロバイダ。
/// ffmpeg コマンドや外部推論スクリプトを子プロセスで起動し、
/// 成果物ファイルを workDir へ受け取る。
class SidecarProvider : public IGenerationProvider
{
public:
    QString id() const override { return QStringLiteral("sidecar"); }
    QString displayName() const override { return QStringLiteral("Sidecar Process"); }

    ProviderCapability capabilities() const override
    {
        ProviderCapability c;
        c.audio = true;
        c.subtitle = true;
        c.offline = true;
        return c;
    }

    bool isAvailable(QString* errorOut) const override;

    GenerationOutput run(const GenerationInput& input,
                         const ProgressFn& progress,
                         const CancelCheckFn& cancelled) override;
};

#if defined(YAVE_ENABLE_ONNX_LOCAL)
/// ONNX Runtime (DirectML / CUDA / CoreML EP) によるローカル推論。
/// ランタイム DLL が無い環境では isAvailable() が false を返す。
class OnnxLocalProvider : public IGenerationProvider
{
public:
    QString id() const override { return QStringLiteral("onnx-local"); }
    QString displayName() const override { return QStringLiteral("Local ONNX Runtime"); }

    ProviderCapability capabilities() const override
    {
        ProviderCapability c;
        c.video = true;
        c.image = true;
        c.audio = true;
        c.offline = true;
        return c;
    }

    bool isAvailable(QString* errorOut) const override;
    GenerationOutput run(const GenerationInput& input,
                         const ProgressFn& progress,
                         const CancelCheckFn& cancelled) override;

    /// モデル検索パス: <userData>/models/
    static void setModelSearchPath(const QString& dir);
};
#endif

} // namespace yave::ai
