#include "AudioRenderGraph.h"
#include "DelayCompensator.h"

#include "../core/AudioClip.h"
#include "../core/Project.h"
#include "../core/Rational.h"
#include "../core/Timeline.h"
#include "../core/Track.h"

namespace yave::audio {

std::unique_ptr<AudioRenderGraph> AudioRenderGraphBuilder::build(
    const Timeline& timeline, const Project& project)
{
    auto graph = std::make_unique<AudioRenderGraph>();
    graph->sampleRate     = project.sampleRate();
    graph->maxBlockFrames = 512;
    graph->masterGain     = float(project.masterGain());

    const Rational tb = timeline.timebase();
    const double framesToSamples = double(graph->sampleRate) / tb.toDouble();

    bool anySolo = false;
    for (const Track* t : timeline.tracksOfType(TrackType::Audio)) {
        if (t->isSolo())
            anySolo = true;
    }
    graph->anySolo = anySolo;

    for (const Track* track : timeline.tracksOfType(TrackType::Audio)) {
        TrackNode node;
        node.gain  = float(track->gain());
        node.pan   = float(track->pan());
        node.muted = track->isMuted();
        node.solo  = track->isSolo();

        for (const auto& clipBase : track->clips()) {
            if (clipBase->type() != ClipType::Audio)
                continue;
            const auto* clip = static_cast<const AudioClip*>(clipBase.get());
            if (!clip->isEnabled())
                continue;

            ClipSource src;
            const TimeRange r = clip->range();
            src.timelineStart = int64_t(double(r.start) * framesToSamples);
            src.timelineEnd   = int64_t(double(r.end()) * framesToSamples);
            src.sourceOffset  =
                int64_t(double(clip->sourceOffset()) * framesToSamples);
            src.gain = float(clip->gain());
            src.pan  = float(clip->pan());
            src.fadeInSamples =
                int64_t(double(clip->fadeInFrames()) * framesToSamples);
            src.fadeOutSamples =
                int64_t(double(clip->fadeOutFrames()) * framesToSamples);
            node.clips.push_back(src);
        }

        // エフェクトチェーン (VST3 ノードは IAudioEffectNode 経由)
        for (IAudioEffectNode* fx : track->effectChain())
            node.effectChain.push_back(fx);

        graph->tracks.push_back(std::move(node));
    }

    // PDC 計算 (UI スレッド側。RT 開始前に compensationDelay を確定させる)
    DelayCompensator::compute(*graph);

    return graph;
}

} // namespace yave::audio
