#pragma once

// FFmpeg RAII ヘルパの前方宣言集。
//
// 実際の deleter 定義は VideoDecoder.h 内 (YAVE_HAVE_FFMPEG 有効時)。
// このヘッダは media モジュール内の他クラスが FFmpeg の有無に依らず
// 同じ include 名で参照できるようにするためのもの。
#include "VideoDecoder.h"
