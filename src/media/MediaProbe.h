#pragma once

#include "MediaInfo.h"

namespace yave::media {

/// ファイルのストリーム情報取得。
///
/// FFmpeg が無効なビルドでは常に ok == false の結果を返す
/// (取り込みダイアログは警告を表示して続行する)。
class MediaProbe
{
public:
    static MediaInfo probe(const QString& filePath);
};

} // namespace yave::media
