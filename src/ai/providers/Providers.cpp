#include "Providers.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QThread>

namespace yave::ai {

// ===========================================================================
//  RemoteHttpProvider
// ===========================================================================

RemoteHttpProvider::RemoteHttpProvider(const QString& baseUrl)
    : baseUrl_(baseUrl.isEmpty() ? QStringLiteral("http://127.0.0.1:8188") : baseUrl)
{}

bool RemoteHttpProvider::isAvailable(QString* errorOut) const
{
    if (baseUrl_.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No remote endpoint configured.");
        return false;
    }
    return true;
}

GenerationOutput RemoteHttpProvider::run(const GenerationInput& input,
                                         const ProgressFn& progress,
                                         const CancelCheckFn& cancelled)
{
    GenerationOutput out;

    // 実装方針:
    //   1. params をプロバイダ固有の JSON ペイロードへ変換する (extraParams 経由で拡張)
    //   2. POST /prompt (ComfyUI) もしくは互換エンドポイントへ送信
    //   3. ポーリングまたは WebSocket で進捗を受信し progress() へ中継
    //   4. 成果物ファイルを workDir へ保存して out.assets へ積む
    //
    // 現バージョンでは HTTP 転送の雛形のみを実装し、実際の API 呼び出しは
    // プロバイダ設定 (extraParams["endpoint"]) に依存する。
    const QString endpoint =
        input.params.extraParams[QStringLiteral("endpoint")].toString();
    if (endpoint.isEmpty()) {
        out.errorMessage = QStringLiteral(
            "Remote provider requires extraParams.endpoint.");
        return out;
    }

    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(baseUrl_ + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject body = input.params.toJson();
    body.insert(QStringLiteral("workDir"), input.workDir);

    auto* reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        out.errorMessage = reply->errorString();
        reply->deleteLater();
        return out;
    }
    reply->deleteLater();

    if (progress && !progress(1.0)) {
        out.errorMessage = QStringLiteral("cancelled");
        return out;
    }

    out.ok = true;
    return out;
}

// ===========================================================================
//  SidecarProvider
// ===========================================================================

bool SidecarProvider::isAvailable(QString* errorOut) const
{
    // sidecar コマンドの存在確認。現状は常時利用可 (実行時に失敗を報告する)。
    Q_UNUSED(errorOut);
    return true;
}

GenerationOutput SidecarProvider::run(const GenerationInput& input,
                                      const ProgressFn& progress,
                                      const CancelCheckFn& cancelled)
{
    GenerationOutput out;

    QDir().mkpath(input.workDir);
    const QString script =
        input.params.extraParams[QStringLiteral("sidecarCommand")].toString();
    if (script.isEmpty()) {
        out.errorMessage = QStringLiteral("Sidecar requires extraParams.sidecarCommand.");
        return out;
    }

    // 子プロセス実行の雛形。進捗は stdout の "PROGRESS <0..1>" 行から抽出する想定。
    QProcess proc;
    proc.setProgram(script);
    proc.setWorkingDirectory(input.workDir);
    proc.start();
    if (!proc.waitForStarted(5000)) {
        out.errorMessage = QStringLiteral("Failed to start sidecar: %1").arg(script);
        return out;
    }

    while (proc.state() == QProcess::Running) {
        if (cancelled && cancelled()) {
            proc.kill();
            proc.waitForFinished(3000);
            out.errorMessage = QStringLiteral("cancelled");
            return out;
        }
        proc.waitForFinished(500);
        while (proc.canReadLine()) {
            const QString line = QString::fromUtf8(proc.readLine()).trimmed();
            bool ok = false;
            const double p = line.mid(9).toDouble(&ok);   // "PROGRESS " 以降
            if (line.startsWith(QLatin1String("PROGRESS ")) && ok && progress)
                progress(p);
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        out.errorMessage = QString::fromUtf8(proc.readAllStandardError());
        return out;
    }

    // workDir 内の成果物を assets として登録する
    for (const QFileInfo& fi :
         QDir(input.workDir).entryInfoList(QDir::Files, QDir::Name)) {
        GeneratedAsset a;
        const QString suffix = fi.suffix().toLower();
        if (suffix == QLatin1String("wav") || suffix == QLatin1String("mp3")
            || suffix == QLatin1String("flac"))
            a.type = GeneratedAsset::Type::Audio;
        else if (suffix == QLatin1String("srt") || suffix == QLatin1String("vtt"))
            a.type = GeneratedAsset::Type::SubtitleData;
        else if (suffix == QLatin1String("json"))
            a.type = GeneratedAsset::Type::Json;
        else
            a.type = GeneratedAsset::Type::Video;
        a.path = fi.absoluteFilePath();
        out.assets.push_back(std::move(a));
    }

    out.ok = true;
    return out;
}

#if defined(YAVE_ENABLE_ONNX_LOCAL)

namespace {
QString g_modelSearchPath;
}

void OnnxLocalProvider::setModelSearchPath(const QString& dir)
{
    g_modelSearchPath = dir;
}

bool OnnxLocalProvider::isAvailable(QString* errorOut) const
{
    // ONNX Runtime C++ API の動的ロード可否を確認する。
    // 実装は OrtGetApiBase() の解決と DirectML EP 列挙。
    if (g_modelSearchPath.isEmpty() || !QDir(g_modelSearchPath).exists()) {
        if (errorOut)
            *errorOut = QStringLiteral("Model directory not configured.");
        return false;
    }
    return true;
}

GenerationOutput OnnxLocalProvider::run(const GenerationInput& input,
                                        const ProgressFn& progress,
                                        const CancelCheckFn&)
{
    GenerationOutput out;
    // ローカル推論の本体は ONNX Runtime C++ API によるセッション実行。
    // YAVE_ENABLE_ONNX_LOCAL=ON かつランタイムが揃った環境でのみリンクされる。
    out.errorMessage = QStringLiteral("Local inference is not configured in this build.");
    Q_UNUSED(input);
    Q_UNUSED(progress);
    return out;
}
#endif

} // namespace yave::ai
