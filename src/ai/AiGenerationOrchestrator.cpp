#include "AiGenerationOrchestrator.h"
#include "AiGenerationTask.h"

#include "../core/AiPlaceholderClip.h"
#include "../core/AudioClip.h"
#include "../core/Clip.h"
#include "../core/Project.h"
#include "../core/Timeline.h"
#include "../core/Track.h"
#include "../core/VideoClip.h"
#include "../core/commands/AddClipCommand.h"
#include "../core/commands/AddTrackCommand.h"
#include "../core/commands/CommitGeneratedAssetCommand.h"

#include <QDir>
#include <QFutureWatcher>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace yave::ai {

// ===========================================================================
//  参照フレーム書き出しブリッジ (app 層が render 側の実装を接続する)
// ===========================================================================

namespace {
ReferenceFrameSink g_referenceFrameSink;
}

void setReferenceFrameSink(ReferenceFrameSink fn)
{
    g_referenceFrameSink = std::move(fn);
}

bool hasReferenceFrameSink()
{
    return bool(g_referenceFrameSink);
}

// ===========================================================================
//  構築 / 破棄
// ===========================================================================

AiGenerationOrchestrator::AiGenerationOrchestrator(Project* project,
                                                   render::RhiCompositor* compositor,
                                                   QObject* parent)
    : QObject(parent)
    , project_(project)
    , compositor_(compositor)
    , registry_(&ProviderRegistry::instance())
    , cache_(new GenerationCache(cacheRoot()))
{
    // 既定は 2 ワーカ。生成中も編集・再生を継続できる (非機能要件 6)。
    pool_.setMaxThreadCount(qMax(1, QThread::idealThreadCount() / 4));
}

AiGenerationOrchestrator::~AiGenerationOrchestrator()
{
    cancelAll();
    pool_.waitForDone(5000);
}

QString AiGenerationOrchestrator::cacheRoot() const
{
    if (!project_)
        return QStringLiteral(".yave_cache/gen");
    return project_->property("projectDir").toString()
               + QStringLiteral("/.yave_cache/gen");
}

// ===========================================================================
//  投入
// ===========================================================================

QUuid AiGenerationOrchestrator::submit(const AiGenerationParams& params)
{
    const auto validation = params.validate();
    if (!validation.ok) {
        emit taskFailed(QUuid(), validation.errorKey);
        return QUuid();
    }

    auto task = std::make_unique<AiGenerationTask>(params);
    const QUuid taskId = task->id();

    // プレースホルダを即座に配置する (Undo 可能)。生成には時間がかかるため、
    // ユーザーは「試して、気に入らなければ戻す」使い方をする。
    if (project_) {
        Timeline* tl = project_->timeline();
        int idx = tl->indexOfTrack(params.targetTrackId);
        if (idx < 0)
            idx = tl->trackCount() - 1;
        if (idx < 0) {
            Track* created = tl->appendTrack(TrackType::AiGenerated);
            idx = tl->indexOfTrack(created);
        }
        if (idx >= 0) {
            auto placeholder = std::make_shared<AiPlaceholderClip>(taskId);
            placeholder->setRange(params.range);
            placeholder->setModelId(params.modelId);
            placeholder->setGeneratedByTaskId(taskId);
            auto* cmd = new AddClipCommand(project_, tl->trackAt(idx)->id(), idx,
                                           placeholder);
            project_->undoStack()->push(cmd);
            placeholderByTask_[taskId] = placeholder;
        }
    }

    connect(task.get(), &AiGenerationTask::progressChanged, this,
            [this, taskId](double p) {
                emit taskProgressChanged(taskId, p);
                if (project_)
                    project_->timeline()->updatePlaceholderProgress(taskId, p);
            });

    AiGenerationTask* raw = task.get();
    {
        QMutexLocker lock(&mutex_);
        tasks_.push_back(std::move(task));
    }

    emit taskAdded(taskId);

    runTask(raw);
    return taskId;
}

void AiGenerationOrchestrator::runTask(AiGenerationTask* task)
{
    task->setState(TaskState::Queued);

    const QUuid taskId = task->id();

    auto watcher = new QFutureWatcher<GenerationOutput>(this);
    connect(watcher, &QFutureWatcher<GenerationOutput>::finished, this,
            [this, watcher, taskId]() {
                const GenerationOutput out = watcher->result();
                AiGenerationTask* t = this->task(taskId);
                if (!t) {
                    watcher->deleteLater();
                    return;
                }
                t->setState(TaskState::PostProcessing);

                if (!out.ok || cancelledTasks_.contains(taskId)) {
                    if (!cancelledTasks_.contains(taskId))
                        t->setError(out.errorMessage.isEmpty()
                                        ? QStringLiteral("generation failed")
                                        : out.errorMessage);
                    else
                        t->setState(TaskState::Cancelled);
                    activeCount_.fetch_sub(1);
                    emit activeTaskCountChanged(int(activeCount_));
                    watcher->deleteLater();
                    return;
                }

                t->mutableAssets() = postProcess(t->params(), out);
                t->setState(TaskState::Cached);
                onTaskCached(t);

                activeCount_.fetch_sub(1);
                emit activeTaskCountChanged(int(activeCount_));
                watcher->deleteLater();
            });

    const AiGenerationParams snapshotParams = task->params();
    activeCount_.fetch_add(1);
    emit activeTaskCountChanged(int(activeCount_));

    task->setState(TaskState::Running);
    watcher->setFuture(QtConcurrent::run(&pool_, [this, task, snapshotParams]()
                                              -> GenerationOutput {
        return executeWithProvider(task, snapshotParams);
    }));
}

// ===========================================================================
//  実行本体 (ワーカスレッド)
// ===========================================================================

GenerationOutput AiGenerationOrchestrator::executeWithProvider(
    AiGenerationTask* task, const AiGenerationParams& params)
{
    GenerationOutput empty;

    IGenerationProvider* provider = registry_->selectFor(params);
    if (!provider) {
        empty.errorMessage = QStringLiteral("No available provider for this task.");
        return empty;
    }

    // キャッシュヒット確認
    const QByteArray hash = params.contentHash();
    const QString cached = cache_->lookup(hash);
    if (!cached.isEmpty()) {
        GeneratedAsset a;
        a.type = (params.kind == GenerationKind::Audio) ? GeneratedAsset::Type::Audio
                                                        : GeneratedAsset::Type::Video;
        a.path = cached;
        a.metadata.insert(QStringLiteral("cacheHit"), true);
        GenerationOutput out;
        out.ok = true;
        out.assets.push_back(a);
        return out;
    }

    QDir().mkpath(workDirFor(task));

    GenerationInput input;
    input.params  = params;
    input.workDir = workDirFor(task);
    input.startRefPath = resolveImageRef(params.startReference
                                             ? *params.startReference : ImageReference{},
                                         params.range.start,
                                         QDir(input.workDir),
                                         QStringLiteral("start_ref.png"));
    input.endRefPath   = resolveImageRef(params.endReference ? *params.endReference
                                                            : ImageReference{},
                                         params.range.end(),
                                         QDir(input.workDir),
                                         QStringLiteral("end_ref.png"));

    auto progressFn = [task](double p) {
        if (task->isCancelRequested())
            return false;
        task->setProgress(p);
        return true;
    };
    auto cancelFn = [task]() { return task->isCancelRequested(); };

    return provider->run(input, progressFn, cancelFn);
}

QString AiGenerationOrchestrator::workDirFor(const AiGenerationTask* task) const
{
    return cacheRoot() + QLatin1Char('/') + task->id().toString(QUuid::WithoutBraces);
}

QString AiGenerationOrchestrator::resolveImageRef(const ImageReference& ref,
                                                  int64_t defaultFrame,
                                                  const QDir& workDir,
                                                  const QString& fileName)
{
    if (ref.source == ImageReference::Source::FilePath && !ref.filePath.isEmpty()) {
        // プロジェクト相対パスを絶対へ解決する
        const QString base =
            project_ ? project_->property("projectDir").toString() : QString();
        const QString abs = base.isEmpty()
                                ? ref.filePath
                                : QDir(base).filePath(ref.filePath);
        return QFileInfo::exists(abs) ? abs : QString();
    }
    if (ref.source == ImageReference::Source::TimelineFrame) {
        // 参照フレームの書き出しは注入された ReferenceFrameSink (render 側実装) を使う。
        // GPU コンテキストが UI スレッドと分離されているため、ここでは要求を積み、
        // メインスレッドの補助でファイル化された画像を待つ。
        if (!hasReferenceFrameSink())
            return {};
        g_referenceFrameSink(ref.sourceTrackId, ref.sourceFrame,
                             workDir.filePath(fileName));
        const QString path = workDir.filePath(fileName);
        for (int i = 0; i < 100; ++i) {           ///< 最大 10 秒待つ
            if (QFileInfo::exists(path))
                return path;
            QThread::msleep(100);
        }
    }
    Q_UNUSED(defaultFrame);
    return {};
}

std::vector<GeneratedAsset> AiGenerationOrchestrator::postProcess(
    const AiGenerationParams& p, const GenerationOutput& raw)
{
    std::vector<GeneratedAsset> out = raw.assets;

    for (auto& a : out) {
        // fps 合わせ: 出力 fps をプロジェクトタイムベースへ変換した尺に合わせる
        if (p.kind == GenerationKind::Video && a.durationFrames > 0) {
            const Rational srcFps = a.frameRate.den > 0 ? a.frameRate : timebase::Fps30;
            a.durationFrames = rescaleFrames(a.durationFrames, srcFps.inverted(),
                                             p.outputFrameRate, RoundMode::Nearest);
            a.frameRate = p.outputFrameRate;
        }
        // seed の書き戻し (ランダム生成時に実際に使われた値を記録する)
        if (p.seed >= 0 && !a.metadata.contains(QStringLiteral("seed")))
            a.metadata.insert(QStringLiteral("seed"), QString::number(p.seed));
    }
    return out;
}

// ===========================================================================
//  コミット / 破棄
// ===========================================================================

void AiGenerationOrchestrator::onTaskCached(AiGenerationTask* task)
{
    emit taskCached(task->id());
    if (autoCommit_)
        commit(task->id());
}

void AiGenerationOrchestrator::commit(const QUuid& taskId)
{
    AiGenerationTask* t = task(taskId);
    if (!t || t->state() != TaskState::Cached || !project_)
        return;

    // 成果物をクリップへ変換してコミットする (Undo 可能)
    auto placeholderIt = placeholderByTask_.find(taskId);

    std::vector<std::shared_ptr<Clip>> generated;
    for (const GeneratedAsset& a : t->assets()) {
        auto clip = makeClipFromAsset(*t, a);
        if (clip)
            generated.push_back(std::move(clip));
    }

    auto* cmd = new CommitGeneratedAssetCommand(
        project_, t->params().targetTrackId,
        placeholderIt != placeholderByTask_.end() ? placeholderIt.value() : nullptr,
        std::move(generated));
    project_->undoStack()->push(cmd);

    t->setState(TaskState::Committed);
    emit taskCommitted(taskId);
}

std::shared_ptr<Clip> AiGenerationOrchestrator::makeClipFromAsset(
    const AiGenerationTask& t, const GeneratedAsset& a)
{
    switch (a.type) {
    case GeneratedAsset::Type::Video:
    case GeneratedAsset::Type::ImageSequence: {
        auto vc = std::make_shared<VideoClip>();
        vc->setRange(t.params().range);
        vc->setGeneratedByTaskId(t.id());
        vc->setName(QStringLiteral("%1 (%2)").arg(t.params().modelId,
                                                  QStringLiteral("AI")));
        return vc;
    }
    case GeneratedAsset::Type::Audio: {
        auto ac = std::make_shared<AudioClip>();
        ac->setRange(t.params().range);
        ac->setGeneratedByTaskId(t.id());
        return ac;
    }
    default:
        break;
    }
    return nullptr;
}

void AiGenerationOrchestrator::discard(const QUuid& taskId)
{
    AiGenerationTask* t = task(taskId);
    if (!t)
        return;

    // プレースホルダを削除し成果物を破棄する
    auto it = placeholderByTask_.find(taskId);
    if (it != placeholderByTask_.end() && project_) {
        Timeline* tl = project_->timeline();
        for (int i = 0; i < tl->trackCount(); ++i) {
            Track* track = tl->trackAt(i);
            if (track && track->clipById(it.value()->id())) {
                auto* cmd = new RemoveClipCommand(project_, track->id(),
                                                  it.value()->id());
                project_->undoStack()->push(cmd);
                break;
            }
        }
        placeholderByTask_.erase(it);
    }
    cache_->remove(t->params().contentHash());
    t->setState(TaskState::Cancelled);
}

// ===========================================================================
//  制御
// ===========================================================================

void AiGenerationOrchestrator::cancel(const QUuid& taskId)
{
    cancelledTasks_.insert(taskId);
    if (AiGenerationTask* t = task(taskId)) {
        t->requestCancel();
        t->setState(TaskState::Cancelled);
    }
}

void AiGenerationOrchestrator::cancelAll()
{
    QMutexLocker lock(&mutex_);
    for (const auto& t : tasks_) {
        t->requestCancel();
    }
}

bool AiGenerationOrchestrator::waitForDone(int timeoutMs)
{
    return pool_.waitForDone(timeoutMs);
}

QUuid AiGenerationOrchestrator::regenerate(const QUuid& taskId, bool newSeed)
{
    AiGenerationTask* old = task(taskId);
    if (!old)
        return QUuid();
    AiGenerationParams p = old->params();
    if (newSeed)
        p.seed = -1;
    return submit(p);
}

void AiGenerationOrchestrator::retry(const QUuid& taskId)
{
    if (AiGenerationTask* t = task(taskId)) {
        t->incrementRetryCount();
        runTask(t);
    }
}

AiGenerationTask* AiGenerationOrchestrator::task(const QUuid& id) const
{
    QMutexLocker lock(&mutex_);
    for (const auto& t : tasks_)
        if (t->id() == id)
            return t.get();
    return nullptr;
}

std::vector<AiGenerationTask*> AiGenerationOrchestrator::tasks() const
{
    QMutexLocker lock(&mutex_);
    std::vector<AiGenerationTask*> out;
    out.reserve(tasks_.size());
    for (const auto& t : tasks_)
        out.push_back(t.get());
    return out;
}

int AiGenerationOrchestrator::activeTaskCount() const
{
    return int(activeCount_.load());
}

} // namespace yave::ai
