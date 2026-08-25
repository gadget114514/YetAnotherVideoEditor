#include "PlaybackController.h"

#include "../../core/Project.h"
#include "../../core/Timeline.h"

namespace yave {

PlaybackController& PlaybackController::instance()
{
    static PlaybackController s;
    return s;
}

PlaybackController::PlaybackController(QObject* parent)
    : QObject(parent)
{
    connect(&audio::AudioRenderEngine::instance(), &audio::AudioRenderEngine::playbackStateChanged,
            this, &PlaybackController::playbackStateChanged);
    connect(&audio::AudioRenderEngine::instance(), &audio::AudioRenderEngine::latencyChanged,
            this, &PlaybackController::latencyChanged);
}

void PlaybackController::attachProject(Project* project)
{
    project_ = project;
    if (!project_)
        return;

    audio::AudioRenderEngine& engine = audio::AudioRenderEngine::instance();
    engine.setTimebase(project_->timebase());
    rebuildGraphIfPossible();

    // Timeline 構造変更 -> 音声グラフ再構築 (再生を止めずに差し替え)
    connect(project_->timeline(), &Timeline::structureChanged, this,
            &PlaybackController::rebuildGraphIfPossible, Qt::UniqueConnection);

    // タイムラインの長さが変わったら QML のルーラ / シークバーへ通知する
    connect(project_->timeline(), &Timeline::durationChanged, this,
            &PlaybackController::durationChanged, Qt::UniqueConnection);
    emit durationChanged(duration());
}

void PlaybackController::rebuildGraphIfPossible()
{
    if (!project_ || !project_->timeline())
        return;
    audio::AudioRenderEngine::instance().rebuildGraph(*project_->timeline(),
                                                      *project_);
}

void PlaybackController::play()
{
    if (project_)
        audio::AudioRenderEngine::instance().play(project_->playhead());
}

void PlaybackController::pause()
{
    audio::AudioRenderEngine::instance().pause();
}

void PlaybackController::stop()
{
    audio::AudioRenderEngine::instance().stop();
}

void PlaybackController::seek(qint64 frame)
{
    audio::AudioRenderEngine::instance().seek(frame);
    if (project_)
        project_->setPlayhead(frame);
}

bool PlaybackController::isPlaying() const
{
    return audio::AudioRenderEngine::instance().isPlaying();
}

qint64 PlaybackController::currentFrame() const
{
    return audio::AudioRenderEngine::instance().currentFrame();
}

double PlaybackController::fps() const
{
    if (project_) {
        const Rational tb = project_->timebase();
        if (tb.num > 0)
            return double(tb.den) / double(tb.num);
    }
    return 60.0;
}

qint64 PlaybackController::duration() const
{
    return project_ && project_->timeline()
               ? project_->timeline()->duration()
               : 0;
}

void PlaybackController::setLoopRange(qint64 start, qint64 end, bool enabled)
{
    audio::AudioRenderEngine::instance().setLoopRange({start, end - start}, enabled);
}

} // namespace yave
