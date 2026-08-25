#pragma once

#include "../ai/AiGenerationParams.h"
#include "../ai/AiGenerationTask.h"
#include "../core/AssetLibrary.h"
#include "../core/BlendMode.h"
#include "../core/Clip.h"
#include "../core/TrackType.h"

#include <QString>

#include <utility>
#include <vector>

namespace yave::io {

template <typename E>
struct EnumMap
{
    static const std::vector<std::pair<E, const char*>>& table();
};

template <typename E>
QString enumToString(E v)
{
    for (const auto& [e, s] : EnumMap<E>::table())
        if (e == v)
            return QString::fromLatin1(s);
    return {};
}

template <typename E>
E enumFromString(const QString& s, E fallback)
{
    for (const auto& [e, str] : EnumMap<E>::table())
        if (s == QLatin1String(str))
            return e;
    return fallback;
}

// ===========================================================================
//  特殊化
// ===========================================================================

#define YAVE_DEFINE_ENUM_MAP(EnumType, ...)                                        \
    template <>                                                                    \
    inline const std::vector<std::pair<EnumType, const char*>>&                    \
    EnumMap<EnumType>::table()                                                     \
    {                                                                              \
        static const std::vector<std::pair<EnumType, const char*>> t = {           \
            __VA_ARGS__                                                            \
        };                                                                         \
        return t;                                                                  \
    }

YAVE_DEFINE_ENUM_MAP(yave::TrackType,
    { yave::TrackType::Video,       "video"       },
    { yave::TrackType::Audio,       "audio"       },
    { yave::TrackType::Subtitle,    "subtitle"    },
    { yave::TrackType::AiGenerated, "aiGenerated" })

YAVE_DEFINE_ENUM_MAP(yave::ClipType,
    { yave::ClipType::Video,         "video"         },
    { yave::ClipType::Audio,         "audio"         },
    { yave::ClipType::Subtitle,      "subtitle"      },
    { yave::ClipType::AiPlaceholder, "aiPlaceholder" },
    { yave::ClipType::Image,         "image"         },
    { yave::ClipType::Color,         "color"         },
    { yave::ClipType::Title,         "title"         })

YAVE_DEFINE_ENUM_MAP(yave::BlendMode,
    { yave::BlendMode::Normal,      "normal"      },
    { yave::BlendMode::Add,         "add"         },
    { yave::BlendMode::Multiply,    "multiply"    },
    { yave::BlendMode::Screen,      "screen"      },
    { yave::BlendMode::Overlay,     "overlay"     },
    { yave::BlendMode::Darken,      "darken"      },
    { yave::BlendMode::Lighten,     "lighten"     },
    { yave::BlendMode::ColorDodge,  "colorDodge"  },
    { yave::BlendMode::ColorBurn,   "colorBurn"   },
    { yave::BlendMode::Difference,  "difference"  },
    { yave::BlendMode::Exclusion,   "exclusion"   },
    { yave::BlendMode::AlphaMask,   "alphaMask"   })

YAVE_DEFINE_ENUM_MAP(yave::ai::GenerationKind,
    { yave::ai::GenerationKind::Video,          "video"          },
    { yave::ai::GenerationKind::Audio,          "audio"          },
    { yave::ai::GenerationKind::Subtitle,       "subtitle"       },
    { yave::ai::GenerationKind::Mask,           "mask"           },
    { yave::ai::GenerationKind::EffectMetadata, "effectMetadata" },
    { yave::ai::GenerationKind::Image,          "image"          })

YAVE_DEFINE_ENUM_MAP(yave::ai::VideoGenMode,
    { yave::ai::VideoGenMode::TextToVideo,  "textToVideo"  },
    { yave::ai::VideoGenMode::ImageToVideo, "imageToVideo" },
    { yave::ai::VideoGenMode::VideoToVideo, "videoToVideo" })

YAVE_DEFINE_ENUM_MAP(yave::ai::I2VReferenceMode,
    { yave::ai::I2VReferenceMode::StartFrameOnly, "startFrameOnly" },
    { yave::ai::I2VReferenceMode::EndFrameOnly,   "endFrameOnly"   },
    { yave::ai::I2VReferenceMode::BothEnds,       "bothEnds"       })

YAVE_DEFINE_ENUM_MAP(yave::ai::AudioGenMode,
    { yave::ai::AudioGenMode::Narration,       "narration"       },
    { yave::ai::AudioGenMode::SoundEffect,     "soundEffect"     },
    { yave::ai::AudioGenMode::Bgm,             "bgm"             },
    { yave::ai::AudioGenMode::VoiceConversion, "voiceConversion" })

YAVE_DEFINE_ENUM_MAP(yave::ai::SubtitleGenMode,
    { yave::ai::SubtitleGenMode::SpeechToText,     "speechToText"     },
    { yave::ai::SubtitleGenMode::ScriptFromPrompt, "scriptFromPrompt" },
    { yave::ai::SubtitleGenMode::Translate,        "translate"        })

YAVE_DEFINE_ENUM_MAP(yave::ai::TaskState,
    { yave::ai::TaskState::Queued,        "queued"        },
    { yave::ai::TaskState::Preparing,     "preparing"     },
    { yave::ai::TaskState::Running,       "running"       },
    { yave::ai::TaskState::PostProcessing,"postProcessing"},
    { yave::ai::TaskState::Cached,        "cached"        },
    { yave::ai::TaskState::Committed,     "committed"     },
    { yave::ai::TaskState::Failed,        "failed"        },
    { yave::ai::TaskState::Cancelled,     "cancelled"     })

YAVE_DEFINE_ENUM_MAP(yave::ai::GeneratedAsset::Type,
    { yave::ai::GeneratedAsset::Type::Video,         "video"         },
    { yave::ai::GeneratedAsset::Type::Audio,         "audio"         },
    { yave::ai::GeneratedAsset::Type::Image,         "image"         },
    { yave::ai::GeneratedAsset::Type::ImageSequence, "imageSequence" },
    { yave::ai::GeneratedAsset::Type::SubtitleData,  "subtitleData"  },
    { yave::ai::GeneratedAsset::Type::Json,          "json"          })

YAVE_DEFINE_ENUM_MAP(yave::Asset::Kind,
    { yave::Asset::Kind::Video,    "video"    },
    { yave::Asset::Kind::Audio,    "audio"    },
    { yave::Asset::Kind::Image,    "image"    },
    { yave::Asset::Kind::Generated,"generated"})

#undef YAVE_DEFINE_ENUM_MAP

} // namespace yave::io

