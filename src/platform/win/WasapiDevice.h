#pragma once

#include "../../audio/IAudioDevice.h"

namespace yave::audio {

/// Windows WASAPI 実装の生成関数。
/// IAudioDevice::create() が YAVE_ENABLE_WASAPI 定義時に呼ぶ。
std::unique_ptr<IAudioDevice> createWasapiDevice();
std::vector<AudioDeviceInfo>  enumerateWasapiDevices();

} // namespace yave::audio
