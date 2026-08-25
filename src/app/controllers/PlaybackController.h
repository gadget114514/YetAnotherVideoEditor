#pragma once

#include "../audio/AudioRenderEngine.h"
#include "../core/Rational.h"

#include <QObject>

namespace yave {

/// 再生制御。オーディオファースト同期 (5.1) の UI 側エントリポイント。
///
/// 映像は AudioRenderEngine::currentFrame() が返す「耳に届いている」位置に
/// 追従する。逆方向 (映像タイマー駆動で音を合わせる) は採らない。
class PlaybackController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playbackStateChanged)
    Q_PROPERTY(qint64 latencySamples READ latencySamples NOTIFY latencyChanged)
    Q_PROPERTY(double fps READ fps NOTIFY durationChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
public:
    static PlaybackController& instance();

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

    /// 再生中のシーク。RT スレッドを止めずに位置を変える。
    Q_INVOKABLE void seek(qint64 frame);

    Q_INVOKABLE bool isPlaying() const;
    Q_INVOKABLE qint64 currentFrame() const;
    Q_INVOKABLE qint64 latencySamples() const {
        return audio::AudioRenderEngine::instance().totalLatencySamples();
    }

    /// プロジェクトのタイムベースから求めた FPS (例: 60000/1001 -> ~59.94)。
    Q_INVOKABLE double fps() const;

    /// タイムライン全体の長さ (フレーム)。ルーラ / シークバーの上限に使う。
    Q_INVOKABLE qint64 duration() const;

    /// ループ再生
    Q_INVOKABLE void setLoopRange(qint64 start, qint64 end, bool enabled);

    /// タイムラインの変更を音声グラフへ反映する (structureChanged で呼ぶ)。
    void rebuildGraphIfPossible();

    void attachProject(Project* project);

signals:
    void playbackStateChanged(bool playing);
    void latencyChanged(qint64 totalSamples);
    void durationChanged(qint64 frames);

private:
    explicit PlaybackController(QObject* parent = nullptr);

    Project* project_ = nullptr;
};

} // namespace yave
